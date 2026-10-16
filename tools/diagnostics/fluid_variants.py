"""Generate reproducible specialization candidates without changing runtime shaders."""
import argparse
import pathlib
import re


def generate(source):
    candidate = source.replace("params.mode", "dispatch_mode()")
    declaration = re.search(r"}\s*params;", candidate)
    if declaration is None:
        raise ValueError("shader push constants missing")
    helper = """
layout(constant_id=0) const uint DIAGNOSTIC_MODE=0xffffffffu;
uint dispatch_mode() {return DIAGNOSTIC_MODE==0xffffffffu?params.mode:DIAGNOSTIC_MODE;}
"""
    return candidate[:declaration.end()] + helper + candidate[declaration.end():]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]
    args.output.mkdir(parents=True, exist_ok=True)
    for name in ("fluid", "projection", "mpm"):
        path = args.output / (name + "_specialized.comp")
        path.write_text(generate((root / "shaders/sim" / (name + ".comp")).read_text()))
        print(path)
    body = (root / "shaders/sim/rigid_body.glsl").read_text()
    predicate = "bool pixel_body(Body b) {return (b.piece_count&PIXEL_BODY_FLAG)!=0u;}"
    if body.count(predicate) != 1:
        raise ValueError("pixel-only compiler experiment predicate changed")
    (args.output / "rigid_body.glsl").write_text(
        body.replace(predicate, "bool pixel_body(Body b) {return true;}"))
    (args.output / "rigid_pixel.comp").write_text(
        (root / "shaders/sim/rigid.comp").read_text())


if __name__ == "__main__":
    main()
