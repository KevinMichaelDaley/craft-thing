// Run from LibreSprite's scripting menu. This exports the ACTIVE document,
// including unsaved edits, as a separate lossless snapshot for the packer.
// Optional explicit absolute staging path; empty uses a sibling of the source.
var CRAFT_PROP_SNAPSHOT = '';
var CRAFT_PROP_OPTIONS = {
    physics_animation: 'visual_only', pivot: [0, 0],
    physical_scale: [1, 1], visual_scale: [1, 1],
    emission_multiplier: 1, relief_scale: 1, allow_invisible_physics: false
};
(function () {
    var sprite = app.activeSprite;
    if (!sprite) throw new Error('Open a multilayer document before exporting');
    var source = sprite.filename;
    if (!source && !CRAFT_PROP_SNAPSHOT)
        throw new Error('Save once to choose a directory, or set CRAFT_PROP_SNAPSHOT. Unsaved edits are always captured.');
    var snapshot = CRAFT_PROP_SNAPSHOT || source + '.craft-export.aseprite';
    if (snapshot === source) throw new Error('Export snapshot must differ from the source document');
    var seen = {};
    for (var i = 0; i < sprite.layerCount; ++i) {
        var layer = sprite.layer(i);
        if (!layer.isImage) continue;
        if (seen[layer.name]) throw new Error('Duplicate layer name: ' + layer.name);
        seen[layer.name] = true;
    }
    if (!seen['physics.material'] || !seen['physics.role'] || !seen.color)
        throw new Error('Required layers: physics.material, physics.role, color (metadata may be hidden)');
    sprite.saveAs(snapshot, true);
    if (sprite.filename !== source) throw new Error('Host failed nondestructive SaveFileCopyAs contract');
    var request = {schema: 'craft.prop-export-request', version: 1,
        exporter: 'craft-libresprite-1', host: 'LibreSprite-1.3',
        source: source, snapshot: snapshot, options: CRAFT_PROP_OPTIONS};
    storage.set(JSON.stringify(request), 'request', 'craft-prop-export');
    var requestPath = storage.save('request', 'craft-prop-export');
    if (!requestPath) throw new Error('Could not publish export request');
    console.log('CRAFT_PROP_REQUEST=' + requestPath);
    console.log('Compile with: python3 scripts/props/pack_prop.py --request "' + requestPath + '" --output "' + source + '.prop"');
})();
