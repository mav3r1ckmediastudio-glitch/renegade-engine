param([Parameter(Mandatory=$true)][string]$Pack,[Parameter(Mandatory=$true)][string]$Output)
# Technical asset adaptation, not new artwork. Source mask is sRGB in the Unity
# importer; reproduce linear sampling + Liquid/Errosion alpha (.2 softness)
# and the prefab's Custom1.x Hermite curve. Resample its 16-frame sequence into
# 64 frames at 256px each for smoother playback, with matching normal tiles.
Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
public static class KnifeMaskCompiler {
 static byte[] Read(Bitmap b) {
  var d=b.LockBits(new Rectangle(0,0,b.Width,b.Height),ImageLockMode.ReadOnly,PixelFormat.Format32bppArgb);
  var p=new byte[d.Stride*b.Height];Marshal.Copy(d.Scan0,p,0,p.Length);b.UnlockBits(d);return p;
 }
 static void Save(byte[] p,string output) {
  using(var b=new Bitmap(2048,2048,PixelFormat.Format32bppArgb)) {
   var d=b.LockBits(new Rectangle(0,0,2048,2048),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);
   Marshal.Copy(p,0,d.Scan0,p.Length);b.UnlockBits(d);b.Save(output,ImageFormat.Png);
  }
 }
 static double Linear(double v) {return v<=.04045?v/12.92:Math.Pow((v+.055)/1.055,2.4);}
 static double Sample(byte[] p,int frame,int x,int y,int channel) {
  int sx=(frame%4)*512+x*2,sy=(frame/4)*512+y*2;
  int i=(sy*2048+sx)*4+channel;
  return (p[i]+p[i+4]+p[i+8192]+p[i+8196])/1020.0;
 }
 public static void Convert(string maskPath,string normalPath,string output,string normalOutput) {
  using(var mask=new Bitmap(maskPath))using(var normal=new Bitmap(normalPath)) {
   if(mask.Width!=2048 || mask.Height!=2048 || normal.Width!=2048 || normal.Height!=2048)
    throw new Exception("Expected matching authored 4x4 2048 sheets.");
   var m=Read(mask);var n=Read(normal);var rgba=new byte[2048*2048*4];var normals=new byte[rgba.Length];
   for(int frame=0;frame<64;frame++){
    double t=frame/63.0, f=t*15;int a=(int)Math.Floor(f),b=Math.Min(15,a+1);double mix=f-a;
    double u=Math.Max(0,Math.Min(1,(t-.25768325)/(1-.25768325)));
    double erosion=.008621216+(1-.008621216)*u*u*(3-2*u);
    for(int y=0;y<256;y++)for(int x=0;x<256;x++){
     int i=(((frame/8)*256+y)*2048+(frame%8)*256+x)*4;
     double red=Linear(Sample(m,a,x,y,2))*(1-mix)+Linear(Sample(m,b,x,y,2))*mix;
     rgba[i]=rgba[i+1]=rgba[i+2]=255;
     rgba[i+3]=(byte)Math.Round(255*Math.Max(0,Math.Min(1,(red-erosion)/.2)));
     double[] v=new double[3];double length=0;
     for(int c=0;c<3;c++){v[c]=(Sample(n,a,x,y,c)*(1-mix)+Sample(n,b,x,y,c)*mix)*2-1;length+=v[c]*v[c];}
     length=Math.Sqrt(Math.Max(.000001,length));
     for(int c=0;c<3;c++)normals[i+c]=(byte)Math.Round(255*(v[c]/length*.5+.5));
     normals[i+3]=255;
    }
   }
   Save(rgba,output);Save(normals,normalOutput);
  }
 }
}
'@
New-Item -ItemType Directory -Force -Path $Output | Out-Null
[KnifeMaskCompiler]::Convert((Join-Path $Pack 'Particles\Textures\Sheets\Blood_1-8.png'),(Join-Path $Pack 'Particles\Textures\Sheets\Blood_1-8_n.png'),(Join-Path $Output 'KnifeBloodSheet64.png'),(Join-Path $Output 'KnifeBloodSheetNormal64.png'))
Get-FileHash (Join-Path $Output '*64.png') -Algorithm SHA256 | Format-Table -AutoSize
