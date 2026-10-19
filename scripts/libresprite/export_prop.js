// Run from LibreSprite's scripting menu. This exports the ACTIVE document,
// including unsaved edits, as a separate lossless snapshot for the packer.
// Optional explicit absolute staging path; empty uses a sibling of the source.
var CRAFT_PROP_SNAPSHOT = '';
var CRAFT_PROP_OPTIONS = {
    physics_animation: 'visual_only', pivot: [0, 0],
    physical_scale: [1, 1], visual_scale: [1, 1],
    emission_multiplier: 1, relief_scale: 1, allow_invisible_physics: false
};
// SHA256 proves the saved copy matches active raw cels, including unsaved edits.
function craftSha256(input) {
    var constants = [0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
    var hash = [0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19];
    var bytes = new Uint8Array(Math.ceil((input.length + 9) / 64) * 64);
    bytes.set(input); bytes[input.length] = 128;
    var bits = input.length * 8;
    for (var i = 0; i < 8; ++i) { bytes[bytes.length - 1 - i] = bits % 256; bits = Math.floor(bits / 256); }
    function rotate(x, n) { return (x >>> n) | (x << (32 - n)); }
    var words = new Int32Array(64);
    for (var at = 0; at < bytes.length; at += 64) {
        for (i = 0; i < 16; ++i) {
            var p = at + i * 4;
            words[i] = (bytes[p] << 24) | (bytes[p + 1] << 16) | (bytes[p + 2] << 8) | bytes[p + 3];
        }
        for (i = 16; i < 64; ++i) {
            var x = words[i - 15], y = words[i - 2];
            words[i] = (words[i - 16] + (rotate(x,7) ^ rotate(x,18) ^ (x >>> 3))
                       + words[i - 7] + (rotate(y,17) ^ rotate(y,19) ^ (y >>> 10))) | 0;
        }
        var a=hash[0],b=hash[1],c=hash[2],d=hash[3],e=hash[4],f=hash[5],g=hash[6],h=hash[7];
        for (i = 0; i < 64; ++i) {
            var t1 = (h + (rotate(e,6) ^ rotate(e,11) ^ rotate(e,25)) + ((e & f) ^ (~e & g)) + constants[i] + words[i]) | 0;
            var t2 = ((rotate(a,2) ^ rotate(a,13) ^ rotate(a,22)) + ((a & b) ^ (a & c) ^ (b & c))) | 0;
            h=g;g=f;f=e;e=(d+t1)|0;d=c;c=b;b=a;a=(t1+t2)|0;
        }
        var result=[a,b,c,d,e,f,g,h];
        for (i=0;i<8;++i) hash[i]=(hash[i]+result[i])|0;
    }
    var hex='';
    for(i=0;i<8;++i) hex+=('00000000'+(hash[i]>>>0).toString(16)).slice(-8);
    return hex;
}
(function () {
    var sprite = app.activeSprite;
    if (!sprite) throw new Error('Open a multilayer document before exporting');
    var source = sprite.filename;
    if (!source && !CRAFT_PROP_SNAPSHOT)
        throw new Error('Save once to choose a directory, or set CRAFT_PROP_SNAPSHOT. Unsaved edits are always captured.');
    var snapshotBase = CRAFT_PROP_SNAPSHOT || source + '.craft-export';
    if (snapshotBase === source) throw new Error('Export snapshot must differ from the source document');
    // A fresh path also prevents a failed save from reusing old duration/tag data.
    var nonce = Date.now().toString(16) + '-' + Math.floor(Math.random() * 0x100000000).toString(16);
    var snapshot = snapshotBase.replace(/\.aseprite$/i, '') + '.' + nonce + '.aseprite';
    var seen = {}, proof = {canvas: [sprite.width, sprite.height], cels: []};
    var reserved = ['physics.material','physics.role','physics.object','physics.anchor','color','emissive','normal','roughness','relief','markers'];
    for (var i = 0; i < sprite.layerCount; ++i) {
        var layer = sprite.layer(i);
        if (!layer.isImage || reserved.indexOf(layer.name) < 0) continue;
        if (seen[layer.name]) throw new Error('Duplicate layer name: ' + layer.name);
        seen[layer.name] = true;
        for (var j = 0; j < layer.celCount; ++j) {
            var cel = layer.cel(j);
            proof.cels.push({layer: layer.name, frame: cel.frame, xy: [cel.x, cel.y],
                size: [cel.image.width, cel.image.height], sha256: craftSha256(cel.image.getImageData())});
        }
    }
    if (!seen['physics.material'] || !seen['physics.role'] || !seen.color)
        throw new Error('Required layers: physics.material, physics.role, color (metadata may be hidden)');
    sprite.saveAs(snapshot, true);
    if (sprite.filename !== source) throw new Error('Host failed nondestructive SaveFileCopyAs contract');
    var request = {schema: 'craft.prop-export-request', version: 1,
        exporter: 'craft-libresprite-1', host: 'LibreSprite-1.3',
        source: source, snapshot: snapshot, options: CRAFT_PROP_OPTIONS, proof: proof};
    storage.set(JSON.stringify(request), 'request', 'craft-prop-export');
    var requestPath = storage.save('request', 'craft-prop-export');
    if (!requestPath) throw new Error('Could not publish export request');
    console.log('CRAFT_PROP_REQUEST=' + requestPath);
    console.log('Compile with: python3 scripts/props/pack_prop.py --request "' + requestPath + '" --output "' + source + '.prop"');
})();
