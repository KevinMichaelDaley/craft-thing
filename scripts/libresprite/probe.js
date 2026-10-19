var sprite = app.activeSprite;
if (!sprite) throw new Error('Open a document before running the probe');
var output = {canvas: [sprite.width, sprite.height], layers: []};
for (var i = 0; i < sprite.layerCount; ++i) {
    var layer = sprite.layer(i);
    var record = {name: layer.name, visible: layer.isVisible, editable: layer.isEditable, cels: []};
    for (var j = 0; j < layer.celCount; ++j) {
        var cel = layer.cel(j);
        record.cels.push({frame: cel.frame, xy: [cel.x, cel.y],
            size: [cel.image.width, cel.image.height], format: cel.image.format,
            pixels: Array.prototype.slice.call(cel.image.getImageData())});
    }
    output.layers.push(record);
}
console.log(JSON.stringify(output));
storage.set(JSON.stringify(output), 'probe', 'craft-prop-probe');
console.log('CRAFT_PROBE_PATH=' + storage.save('probe', 'craft-prop-probe'));
sprite.saveAs('__PROBE_OUTPUT__.aseprite', true);
