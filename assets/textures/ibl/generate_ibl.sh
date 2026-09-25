#!/bin/bash
CMGEN="/Users/sakakibarayuto/Vanta/tools/filament/bin/cmgen"
HDR_SRC="/Users/sakakibarayuto/Vanta/assets/textures/studio_small_04_4k.hdr"
OUT_DIR="/Users/sakakibarayuto/Vanta/assets/textures/ibl/studio_small_04_4k"

mkdir -p "$OUT_DIR"
sizes=(256 128 64 32 16 8)

for i in "${!sizes[@]}"; do
    size=${sizes[$i]}
    echo "Generating Mip $i (size $size)"
    mkdir -p tmp_out
    $CMGEN --format=hdr --size=$size --extract=tmp_out $HDR_SRC > /dev/null
    
    # Rename and move to OUT_DIR
    mv tmp_out/studio_small_04_4k/px.hdr "$OUT_DIR/m${i}_px.hdr"
    mv tmp_out/studio_small_04_4k/nx.hdr "$OUT_DIR/m${i}_nx.hdr"
    mv tmp_out/studio_small_04_4k/py.hdr "$OUT_DIR/m${i}_py.hdr"
    mv tmp_out/studio_small_04_4k/ny.hdr "$OUT_DIR/m${i}_ny.hdr"
    mv tmp_out/studio_small_04_4k/pz.hdr "$OUT_DIR/m${i}_pz.hdr"
    mv tmp_out/studio_small_04_4k/nz.hdr "$OUT_DIR/m${i}_nz.hdr"
    
    rm -rf tmp_out
done

echo "Done!"
