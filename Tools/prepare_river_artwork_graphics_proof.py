"""Optional original-artwork foam proof using the validated transport harness."""
from pathlib import Path
import subprocess,sys
root=Path(__file__).resolve().parents[1];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
subprocess.run([sys.executable,str(root/'Tools/prepare_river_strands_graphics_proof.py'),str(out)],check=True)
s=(out/'RiverFoamStrandsProof.cpp').read_text()
s=s.replace('Renegade River V2 - Advected Foam Strands','Renegade River V2 - Original Foam Detail')
s=s.replace('ADVECTED FOAM STRANDS / ','ORIGINAL FOAM DETAIL / ').replace('ADVECTED STRANDS','ORIGINAL FROTH')
s=s.replace('Source-born detail follows actual flow; same displaced WATER. B compares broad coverage with transported strands.','Original foam artwork with actual source and velocity fields. B broad coverage / original froth. Space pause; V waves.')
s=s.replace('XMUINT4 strands;};','XMUINT4 strands;XMUINT4 artwork;};').replace('sizeof(Params)==112','sizeof(Params)==128')
s=s.replace('detail[2],detailForward;','detail[2],detailForward,artwork;')
s=s.replace('gpu->BindResource(&foam[state],4,cmd);','gpu->BindResource(&foam[state],4,cmd);gpu->BindResource(&artwork,5,cmd);')
anchor='        Params params{'
assert s.count(anchor)==1
s=s.replace(anchor,'''        auto originalArtwork=wi::resourcemanager::Load((fs::absolute(argv[3])/"OriginalFoamCoverage.png").generic_u8string());
        Check(originalArtwork.IsValid(),"Original foam artwork resource");
        path.artwork=originalArtwork.GetTexture();Check(path.artwork.IsValid(),"Original foam artwork texture");
        std::cout<<"ORIGINAL_ARTWORK texture="<<path.artwork.GetDesc().width<<"x"<<path.artwork.GetDesc().height<<" mipLevels="<<path.artwork.GetDesc().mip_levels<<"\\n";
'''+anchor)
a='{descriptor(path.detail[0]),descriptor(path.detail[1]),0,0}};'
assert s.count(a)==1
s=s.replace(a,'{descriptor(path.detail[0]),descriptor(path.detail[1]),0,0},{descriptor(path.artwork),Bits(4),0,0}};')
(out/'RiverFoamArtworkProof.cpp').write_text(s)
print('G8G_ORIGINAL_ARTWORK_HARNESS_STAGED')
