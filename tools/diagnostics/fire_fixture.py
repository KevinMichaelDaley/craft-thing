#!/usr/bin/env python3
"""Validate/export locked fire fixtures and capture the pre-feature live baseline."""
import argparse
import array
import hashlib
import json
import math
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = ROOT / "assets/fixtures/fire_smoke_v1.json"


def scene_data(data, name, seen=()):
    if name in seen or name not in data["scenes"]:
        raise ValueError("Unknown/cyclic fixture inheritance: " + name)
    scene = data["scenes"][name]
    if "inherits" not in scene:
        return dict(scene)
    parent = scene_data(data, scene["inherits"], seen + (name,))
    inherited = [p for p in parent["primitives"] if p.get("object",0) not in scene.get("remove_objects",[])]
    parent.update(scene)
    parent["primitives"] = inherited + scene["primitives"]
    return parent


def validate(data):
    if data.get("schema") != 1 or data.get("view") != [480, 270]:
        raise ValueError("Unsupported fixture version or visible grid")
    if data.get("simulation") != [640, 448] or data.get("halo_cells") != 64:
        raise ValueError("Fixture simulation halo changed")
    controls=data["reaction_controls"]
    if not 0<controls["ambient_oxygen_mass_fraction"]<=1:
        raise ValueError("Invalid ambient oxygen normalization")
    profiles = data["materials"]
    named={p["name"]:p for p in profiles}
    mix=controls["oil_jelly_mix_mass_fractions"]
    if len(mix)!=2 or any(v<0 for v in mix) or abs(sum(mix)-1)>1e-8:
        raise ValueError("Invalid oil/jelly recipe proportions")
    ingredients=[named["oil"],named["red_slime_jelly"]]
    input_fuel=sum(w*p["fuel_fraction"] for w,p in zip(mix,ingredients))
    input_oxidizer=sum(w*p.get("bound_oxidizer_fraction",0) for w,p in zip(mix,ingredients))
    input_energy=sum(w*p["fuel_fraction"]*p["chemical_energy"] for w,p in zip(mix,ingredients))
    compound=named["greek_fire"]
    if abs(input_fuel-compound["fuel_fraction"])>1e-8 or abs(input_oxidizer-compound["bound_oxidizer_fraction"])>1e-8:
        raise ValueError("Compound mixing creates/destroys fuel or oxidizer")
    if compound["fuel_fraction"]*compound["chemical_energy"]>input_energy+1e-6:
        raise ValueError("Compound mixing creates chemical energy")
    ids = {p["id"] for p in profiles}
    names = {p["name"] for p in profiles}
    if len(ids) != len(profiles) or len(names) != len(profiles):
        raise ValueError("Duplicate material identity")
    if ids != set(range(len(profiles))) or len(profiles) != 20:
        raise ValueError("Material identity mapping is incomplete")
    char_energy = next(p["chemical_energy"] for p in profiles if p["name"] == "char")
    positive = ("density", "heat_capacity")
    nonnegative = ("conductivity", "fuel_fraction", "ignition_K", "hazard_per_second",
                   "surface_burn_per_second", "chemical_energy", "primary_heat", "oxygen_per_fuel")
    for p in profiles:
        if any(not math.isfinite(p[k]) or p[k] <= 0 for k in positive):
            raise ValueError("Nonpositive physical profile")
        if any(not math.isfinite(p[k]) or p[k] < 0 for k in nonnegative):
            raise ValueError("Negative/nonfinite reactive profile")
        yields = p["fuel_yields"]
        if len(yields) != 4 or any(not math.isfinite(v) or v < 0 for v in yields) or abs(sum(yields)-1) > 1e-8:
            raise ValueError("Fuel yields do not partition reacted fuel")
        if p["primary_heat"] + yields[0]*char_energy > p["chemical_energy"] + 1e-8:
            raise ValueError("Primary heat plus retained char creates chemical energy")
        if p["fuel_fraction"] + p.get("bound_oxidizer_fraction", 0) > 1 + 1e-8:
            raise ValueError("Fuel and bound oxidizer exceed material mass")
        modifiers=data["material_modifiers"][p["name"]]
        if len(modifiers)!=4 or any(not math.isfinite(v) or v<0 for v in modifiers):
            raise ValueError("Invalid electrical/water/oxidation/corrosion modifiers")
    width, height = data["view"]
    for name in data["scenes"]:
        scene = scene_data(data, name)
        if not isinstance(scene["seed"], int) or scene["duration_seconds"] <= 0:
            raise ValueError("Invalid fixture seed/duration")
        shapes = list(scene["primitives"])
        objects={p.get("object",0) for p in shapes}
        for state in scene.get("initial_states",[]):
            if state["object"] not in objects or state["temperature_K"]<=0:
                raise ValueError("Initial reactive state refers to absent/invalid object")
        sources=list(scene.get("phase_sources",[]))
        for variant in scene.get("phase_source_variants",{}).values():
            sources+=variant
        for source in sources:
            profile=next((p for p in profiles if p["name"]==source["material"]),None)
            if not profile or source["host_object"] not in objects or source["total_mass"]<=0:
                raise ValueError("Invalid phase source material, carrier or mass")
            if abs(source["fuel_mass"]-profile["fuel_fraction"]*source["total_mass"])>1e-6:
                raise ValueError("Phase source fuel disagrees with total composition")
            if abs(source["bound_oxidizer_mass"]-profile.get("bound_oxidizer_fraction",0)*source["total_mass"])>1e-6:
                raise ValueError("Phase source oxidizer disagrees with total composition")
        for variant in scene.get("variants", {}).values():
            shapes += variant
        for shape in shapes:
            if shape["material"] not in names or shape["role"] not in range(5):
                raise ValueError("Unknown material/ownership role")
            if not 0 <= shape.get("wetness", 0) <= 1:
                raise ValueError("Invalid wetness saturation")
            if ("rect" in shape) == ("circle" in shape):
                raise ValueError("Each primitive needs exactly one geometry")
            if "rect" in shape:
                x, y, w, h = shape["rect"]
                if not all(isinstance(v, int) for v in (x,y,w,h)) or x < 0 or y < 0 or min(w,h) <= 0 or x+w > width or y+h > height:
                    raise ValueError("Out-of-bounds rectangle")
            else:
                x, y, radius = shape["circle"]
                if radius <= 0 or x-radius < 0 or y-radius < 0 or x+radius > width or y+radius > height:
                    raise ValueError("Out-of-bounds circle")
    return data


def little_bytes(values):
    if sys.byteorder != "little":
        values.byteswap()
    return values.tobytes()


def build_map(data, name, variant=None):
    scene = scene_data(data, name)
    shapes = list(scene["primitives"])
    if variant is not None:
        if variant not in scene.get("variants", {}):
            raise ValueError("Unknown fixture variant: " + variant)
        shapes += scene["variants"][variant]
    width, height = data["view"]
    cells = width*height
    material = array.array("H", [0])*cells
    role = bytearray(cells)
    objects = array.array("I", [0])*cells
    wetness = array.array("f", [0])*cells
    ids = {p["name"]: p["id"] for p in data["materials"]}
    for shape in shapes:
        if "rect" in shape:
            x, y, w, h = shape["rect"]
            low_x, low_y, high_x, high_y = x, y, x+w, y+h
        else:
            x, y, radius = shape["circle"]
            low_x, low_y = math.floor(x-radius), math.floor(y-radius)
            high_x, high_y = math.ceil(x+radius), math.ceil(y+radius)
        for sy in range(low_y, high_y):
            for sx in range(low_x, high_x):
                if "circle" in shape and (sx+.5-x)**2+(sy+.5-y)**2 > radius*radius:
                    continue
                i = sy*width+sx
                material[i] = ids[shape["material"]]
                role[i] = shape["role"]
                objects[i] = shape.get("object", 0)
                wetness[i] = shape.get("wetness", 0)
    return {"material": little_bytes(material), "role": bytes(role),
            "object": little_bytes(objects), "wetness": little_bytes(wetness)}


def export_maps(data, output):
    digests = {}
    (output/"fixture_manifest.json").write_text(json.dumps(data,indent=2)+"\n")
    for name in data["scenes"]:
        scene = scene_data(data, name)
        for variant in [None] + list(scene.get("variants", {})):
            key = name + "/" + (variant or "default")
            directory = output/key
            directory.mkdir(parents=True)
            digests[key] = {}
            for channel, content in build_map(data, name, variant).items():
                suffix = {"material":"u16", "role":"u8", "object":"u32", "wetness":"f32"}[channel]
                (directory/(channel+"."+suffix)).write_bytes(content)
                digests[key][channel] = hashlib.sha256(content).hexdigest()
    (output/"map_sha256.json").write_text(json.dumps(digests, indent=2)+"\n")
    return digests


def material_c(data):
    """Generate only physical/chemical coefficients; pigment is a separate channel."""
    legacy={"stone":1,"wood":2,"mortar_debris":3,"dense_wood":4,"leaves":5,
            "sand":6,"dirt":7,"gravel":8,"leaf_debris":9,"coal":10,"char":11}
    granular={"sand","dirt","gravel","mortar_debris","leaf_debris","ash","corrosion_residue"}
    liquid={"water","oil","red_slime_jelly","greek_fire"}
    controls=data["reaction_controls"]
    def number(value):
        value=format(float(value),".9g")
        return value+("" if "." in value or "e" in value else ".0")+"f"
    lines=["/* Generated by tools/diagnostics/fire_fixture.py; edit the locked manifest. */",
           "static const fr_reactive_profile_t profiles[] = {"]
    for p in data["materials"]:
        name=p["name"]
        phase="FR_REACTIVE_GAS" if name=="air" else "FR_REACTIVE_GRANULAR" if name in granular else "FR_REACTIVE_LIQUID" if name in liquid else "FR_REACTIVE_SOLID"
        flags=(1 if p["fuel_fraction"]>0 else 0)|(2 if p["fuel_fraction"]>0 and name!="greek_fire" else 0)|(4 if p.get("bound_oxidizer_fraction",0)>0 else 0)
        if phase=="FR_REACTIVE_SOLID" or name in legacy:flags|=8
        fields={key:p[key] for key in ("density","heat_capacity","conductivity","ignition_K","hazard_per_second","fuel_fraction","chemical_energy","primary_heat","oxygen_per_fuel","surface_burn_per_second")}
        fields["bound_oxidizer_fraction"]=p.get("bound_oxidizer_fraction",0)
        for key,value in zip(("electrical_conductivity","moisture_capacity","oxidized_hazard_scale","greek_corrosion_susceptibility"),data["material_modifiers"][name]):fields[key]=value
        fields["saturated_hazard_factor"]=controls["ordinary_wet_hazard_factor_at_full_saturation"]
        fields["oil_hazard_factor"]=controls["oil_coating_hazard_factor_at_full_coverage"]
        fields.update(yield_stress=0,consistency=0,shear_exponent=1,softening_K=0,hot_yield_multiplier=1,cold_adhesion=0,hot_adhesion=0)
        if name in ("oil","red_slime_jelly","greek_fire"):
            fraction=0 if name=="oil" else 1 if name=="red_slime_jelly" else controls["oil_jelly_mix_mass_fractions"][1]
            for key in ("yield_stress","consistency","shear_exponent"):
                fields[key]=(1-fraction)*controls["oil_"+key]+fraction*controls["jelly_"+key]
            fields["softening_K"]=controls["sludge_softening_temperature_K"]
            fields["hot_yield_multiplier"]=controls["hot_yield_stress_multiplier"]
            fields["cold_adhesion"]=controls["cold_film_adhesion_energy_per_mass"]
            fields["hot_adhesion"]=controls["hot_film_adhesion_energy_per_mass"]
        lines.append("    {.material=%uu,.legacy_body_material=%uu,.phase=%s,.flags=%uu,"%(p["id"],legacy.get(name,0),phase,flags))
        items=[".%s=%s"%(key,number(value)) for key,value in fields.items()]
        for offset in range(0,len(items),4):lines.append("     "+",".join(items[offset:offset+4])+",")
        lines.append("     .fuel_yields={"+",".join(number(v) for v in p["fuel_yields"])+"}},")
    lines.append("};")
    lines.append("static const char *const profile_names[] = {")
    lines.extend('    "'+p["name"]+'",' for p in data["materials"])
    lines.append("};")
    lines.append("static const float ambient_oxygen_fraction = "+number(controls["ambient_oxygen_mass_fraction"])+";")
    return "\n".join(lines)+"\n"


def baseline(data, source_world, output):
    source_world = source_world.resolve()
    source_hash = hashlib.sha256((source_world/"rigid_bodies.bin").read_bytes()).hexdigest()
    if source_hash != data["acceptance"]["baseline_world_rigid_sha256"]:
        raise ValueError("Baseline source is not the locked original world")
    if not os.environ.get("WAYLAND_DISPLAY"):
        raise ValueError("An actual Wayland display is required")
    overrides = [key for key in os.environ if key.startswith("DC_")]
    if overrides:
        raise ValueError("Baseline requires default runtime controls: " + ",".join(overrides))
    device = subprocess.run([str(ROOT/"tools/with-vulkan"), "vulkaninfo", "--summary"],
                            cwd=ROOT, text=True, capture_output=True, check=True)
    (output/"vulkan_device.txt").write_text(device.stdout)
    report = {"feature_state":"pre-feature saved-scene baseline only",
              "code_commit":subprocess.check_output(["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(),
              "manifest_sha256":hashlib.sha256(json.dumps(data,sort_keys=True,separators=(",",":")).encode()).hexdigest(),
              "source_world_sha256":source_hash, "runs":[]}
    env = dict(os.environ, SDL_VIDEODRIVER="wayland", DC_BENCHMARK_AIR_BRUSH="225,42,5")
    for index in range(data["acceptance"]["consecutive_runs"]):
        world = output/("world_%u"%index)
        shutil.copytree(source_world, world)
        log = output/("displayed_%u.log"%index)
        capture = output/("displayed_%u.bmp"%index)
        command = [str(ROOT/"tools/with-vulkan"),str(ROOT/"build/dungeoncraft_quarter_native"),
                   "--scene",data["acceptance"]["baseline_scene"],"--world-dir",str(world),
                   "--benchmark-frames",str(data["acceptance"]["measured_frames"]),
                   "--benchmark-capture",str(capture)]
        start = time.monotonic()
        with log.open("w") as stream:
            result = subprocess.run(command,cwd=ROOT,env=env,stdout=stream,stderr=subprocess.STDOUT)
        text = log.read_text()
        match = re.search(r"Displayed full frame (\d+)x(\d+) physics/render.*mean ([\d.]+)ms worst ([\d.]+)ms, (\d+) frames",text)
        if not match:
            raise ValueError("Displayed baseline did not complete; inspect " + str(log))
        run = {"view":[int(match[1]),int(match[2])],"mean_ms":float(match[3]),
               "worst_ms":float(match[4]),"measured_frames":int(match[5]),
               "wall_seconds_including_startup":time.monotonic()-start,"exit_code":result.returncode,
               "paused_captures_identical":"Four paused pixels identical" in text}
        run["budget_pass"] = result.returncode == 0 and run["worst_ms"] < data["acceptance"]["frame_budget_ms"]
        report["runs"].append(run)
        (output/"baseline_report.json").write_text(json.dumps(report,indent=2)+"\n")
        print(json.dumps(run),flush=True)
    if hashlib.sha256((source_world/"rigid_bodies.bin").read_bytes()).hexdigest() != source_hash:
        raise ValueError("Protected source world changed during baseline")
    return all(run["budget_pass"] for run in report["runs"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest",type=pathlib.Path,default=DEFAULT_MANIFEST)
    parser.add_argument("--output-dir",type=pathlib.Path)
    parser.add_argument("--baseline-world",type=pathlib.Path)
    parser.add_argument("--emit-material-c",type=pathlib.Path)
    parser.add_argument("--check-material-c",type=pathlib.Path)
    args = parser.parse_args()
    data = validate(json.loads(args.manifest.read_text()))
    if args.emit_material_c:
        args.emit_material_c.parent.mkdir(parents=True,exist_ok=True)
        args.emit_material_c.write_text(material_c(data))
    if args.check_material_c and args.check_material_c.read_text()!=material_c(data):
        raise ValueError("Generated material coefficients are stale")
    if args.output_dir:
        output = args.output_dir.resolve()
        output.mkdir(parents=True,exist_ok=False)
        export_maps(data,output)
        if args.baseline_world and not baseline(data,args.baseline_world,output):
            return 1
    elif args.baseline_world:
        parser.error("--baseline-world requires a fresh --output-dir")
    print("Validated locked fire/smoke fixtures",flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (ValueError,KeyError,OSError,subprocess.CalledProcessError) as error:
        print("Fire fixture error: "+str(error),file=sys.stderr)
        raise SystemExit(2)
