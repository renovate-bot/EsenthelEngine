/******************************************************************************/
#include "!Header.h"
#include "Cubic.h"
/******************************************************************************/
#define CUBIC_SAMPLES     3
#define CUBIC_RANGE       2
#define CUBIC_SHARPNESS   (2/2.5)
#define CUBIC_QUALITY     2 // 0..2, 0=3.748 fps, 1=3.242 fps, 2=3.075 fps (however when using CUBIC_SKIP_SAMPLE it's now much faster)
#define CUBIC_SKIP_SAMPLE (1 && CUBIC_QUALITY==2) // because the actual range is 2.5 then it means we need to process 5x5 samples (and not 6x6), this optimization can work only if actual range <= 2.5, also we can enable this only for CUBIC_QUALITY==2 because only this mode operates on single 1x1 pixels and not 2x2 blocks)

Flt Cubic(Flt x, Flt blur, Flt sharpen)
{
   Flt x2=x*x,
       x3=x*x*x;
   return (x<=1) ? ((12-9*blur-6*sharpen)/6*x3 + (-18+12*blur+6*sharpen)/6*x2 +                             (6-2*blur         )/6)
                 : ((-blur-6*sharpen    )/6*x3 + (6*blur+30*sharpen    )/6*x2 + (-12*blur-48*sharpen)/6*x + (8*blur+24*sharpen)/6);
}
Half Cubic(Half x, Half blur, Half sharpen)
{
   Half x2=x*x,
        x3=x*x*x;
   return (x<=1) ? ((12-9*blur-6*sharpen)/6*x3 + (-18+12*blur+6*sharpen)/6*x2 +                             (6-2*blur         )/6)
                 : ((-blur-6*sharpen    )/6*x3 + (6*blur+30*sharpen    )/6*x2 + (-12*blur-48*sharpen)/6*x + (8*blur+24*sharpen)/6);
}
Flt  CubicMed(Flt  x) {return Cubic(x, 0.0, 0.375);}
Half CubicMed(Half x) {return Cubic(x, 0.0, 0.375);}

/*VecH4 TexLerp(Flt x0, Flt x1, Flt y, Flt l, Flt r)
{
   Flt w=l+r;
   // keep 'Tex' in case we need LODs (for example stretching in 1 dimension but shrinking in another)
   return Tex(Img, Vec2((x0*l + x1*r)/w, y))*Half(w);
}
VecH TexLerpRGB(Flt x0, Flt x1, Flt y, Flt l, Flt r) // ignores alpha channel
{
   Flt w=l+r;
   // keep 'Tex' in case we need LODs (for example stretching in 1 dimension but shrinking in another)
   return Tex(Img, Vec2((x0*l + x1*r)/w, y)).rgb*Half(w);
}

VecH4 TexLerp(Vec2 t0, Vec2 t1, Flt lu, Flt ru, Flt lb, Flt rb)
{
#if 0 // slower
   return TexPoint(Img, Vec2(t0.x, t0.y))*lu
         +TexPoint(Img, Vec2(t1.x, t0.y))*ru
         +TexPoint(Img, Vec2(t0.x, t1.y))*lb
         +TexPoint(Img, Vec2(t1.x, t1.y))*rb;
#else
   Vec2 t=Vec2(t0.x, t0.y)*lu
         +Vec2(t1.x, t0.y)*ru
         +Vec2(t0.x, t1.y)*lb
         +Vec2(t1.x, t1.y)*rb;
   Flt w=lu+ru+lb+rb;
   // keep 'Tex' in case we need LODs (for example stretching in 1 dimension but shrinking in another)
   return Tex(Img, t/w)*Half(w);
#endif
}
VecH TexLerpRGB(Vec2 t0, Vec2 t1, Flt lu, Flt ru, Flt lb, Flt rb) // ignores alpha channel
{
   Vec2 t=Vec2(t0.x, t0.y)*lu
         +Vec2(t1.x, t0.y)*ru
         +Vec2(t0.x, t1.y)*lb
         +Vec2(t1.x, t1.y)*rb;
   Flt w=lu+ru+lb+rb;
   // keep 'Tex' in case we need LODs (for example stretching in 1 dimension but shrinking in another)
   return Tex(Img, t/w).rgb*Half(w);
}*/

VecH4 TexCubicPlus(Vec2 inTex)
{
   Vec2  pixel =inTex*ImgSize.zw-0.5,
         pixeli=Floor(pixel),
         offset       [CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE];
   VecH2 offset_weight[CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE];

#if CUBIC_SKIP_SAMPLE
   pixeli+=(pixel-pixeli>=CUBIC_RANGE/CUBIC_SHARPNESS-(CUBIC_SAMPLES-1)); // if the left/top coordinate is completely out of range, then process the next pixel (the >= returns Vec2, so it modifies X and Y independently)
#endif

   UNROLL for(int i=0; i<CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE; i++)
   {
      offset       [i]=     pixeli+(i-(CUBIC_SAMPLES-1));
      offset_weight[i]=Sqr((pixel -offset[i])*CUBIC_SHARPNESS);
      offset       [i]=           (offset[i]+0.5)*ImgSize.xy;
   }
   VecH4 color =0;
   Half  weight=0;
#if CUBIC_QUALITY>=2
   UNROLL for(int y=0; y<CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE; y++)
   UNROLL for(int x=0; x<CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE; x++)
   {
      Half w=offset_weight[x].x+offset_weight[y].y; if(w<Sqr(CUBIC_RANGE))
      {
         w=CubicMed(Sqrt(w));
         color +=w*TexPoint(Img, Vec2(offset[x].x, offset[y].y)); // don't use "color+=w*Img.Load(VecI(offset[x].x without scale, offset[y].y without scale, 0)); because it is slower and doesn't support wrap/clamp
         weight+=w;
      }
   }
#else
   Flt weights[CUBIC_SAMPLES*2][CUBIC_SAMPLES*2]; // [y][x]
   UNROLL for(int y=0; y<CUBIC_SAMPLES*2; y++)
   UNROLL for(int x=0; x<CUBIC_SAMPLES*2; x++)
   {
      Flt w=offset_weight[x].x+offset_weight[y].y;
          w=(w<Sqr(CUBIC_RANGE) ? CubicMed(Sqrt(w)) : 0);
      weights[y][x]=w;
      weight      +=w;
   }
   #if CUBIC_QUALITY>=1 // medium quality
      UNROLL for(int y=0; y<CUBIC_SAMPLES*2; y++ )
      UNROLL for(int x=0; x<CUBIC_SAMPLES*2; x+=2)color+=TexLerp(offset[x].x, offset[x+1].x, offset[y].y, weights[y][x], weights[y][x+1]);
   #else // low quality
      UNROLL for(int y=0; y<CUBIC_SAMPLES*2; y+=2)
      UNROLL for(int x=0; x<CUBIC_SAMPLES*2; x+=2)color+=TexLerp(Vec2(offset[x].x, offset[y].y), Vec2(offset[x+1].x, offset[y+1].y), weights[y][x], weights[y][x+1], weights[y+1][x], weights[y+1][x+1]);
   #endif
#endif
   return color/weight;
}
VecH TexCubicPlusRGB(Vec2 inTex) // ignores alpha channel
{
   Vec2  pixel =inTex*ImgSize.zw-0.5,
         pixeli=Floor(pixel),
         offset       [CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE];
   VecH2 offset_weight[CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE];

#if CUBIC_SKIP_SAMPLE
   pixeli+=(pixel-pixeli>=CUBIC_RANGE/CUBIC_SHARPNESS-(CUBIC_SAMPLES-1)); // if the left/top coordinate is completely out of range, then process the next pixel (the >= returns Vec2, so it modifies X and Y independently)
#endif

   UNROLL for(int i=0; i<CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE; i++)
   {
      offset       [i]=     pixeli+(i-(CUBIC_SAMPLES-1));
      offset_weight[i]=Sqr((pixel -offset[i])*CUBIC_SHARPNESS);
      offset       [i]=           (offset[i]+0.5)*ImgSize.xy;
   }
   VecH color =0;
   Half weight=0;
#if CUBIC_QUALITY>=2 // high quality
   UNROLL for(int y=0; y<CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE; y++)
   UNROLL for(int x=0; x<CUBIC_SAMPLES*2-CUBIC_SKIP_SAMPLE; x++)
   {
      Half w=offset_weight[x].x+offset_weight[y].y; if(w<Sqr(CUBIC_RANGE))
      {
         w=CubicMed(Sqrt(w));
         color +=w*TexPoint(Img, Vec2(offset[x].x, offset[y].y)).rgb; // don't use "color+=w*Img.Load(VecI(offset[x].x without scale, offset[y].y without scale, 0)).rgb; because it is slower and doesn't support wrap/clamp
         weight+=w;
      }
   }
#else
   Flt weights[CUBIC_SAMPLES*2][CUBIC_SAMPLES*2]; // [y][x]
   UNROLL for(int y=0; y<CUBIC_SAMPLES*2; y++)
   UNROLL for(int x=0; x<CUBIC_SAMPLES*2; x++)
   {
      Flt w=offset_weight[x].x+offset_weight[y].y;
          w=(w<Sqr(CUBIC_RANGE) ? CubicMed(Sqrt(w)) : 0);
      weights[y][x]=w;
      weight      +=w;
   }
   #if CUBIC_QUALITY>=1 // medium quality
      UNROLL for(int y=0; y<CUBIC_SAMPLES*2; y++ )
      UNROLL for(int x=0; x<CUBIC_SAMPLES*2; x+=2)color+=TexLerpRGB(offset[x].x, offset[x+1].x, offset[y].y, weights[y][x], weights[y][x+1]);
   #else // low quality
      UNROLL for(int y=0; y<CUBIC_SAMPLES*2; y+=2)
      UNROLL for(int x=0; x<CUBIC_SAMPLES*2; x+=2)color+=TexLerpRGB(Vec2(offset[x].x, offset[y].y), Vec2(offset[x+1].x, offset[y+1].y), weights[y][x], weights[y][x+1], weights[y+1][x], weights[y+1][x+1]);
   #endif
#endif
   return color/weight;
}
/******************************************************************************/
VecH4 DrawTexCubicFast_PS(NOPERSP Vec2 inTex:TEXCOORD,
                          NOPERSP PIXEL              ):TARGET
{
   VecH4 col=TexCubicFast(Img, inTex);
#if COLORS
   col=col*Color[0]+Color[1];
#endif
#if DITHER
   ApplyDither(col.rgb, pixel.xy);
#endif
   return col;
}
VecH4 DrawTexCubicFastRGB_PS(NOPERSP Vec2 inTex:TEXCOORD,
                             NOPERSP PIXEL              ):TARGET
{
   VecH4 col=VecH4(TexCubicFastRGB(Img, inTex), 1);
#if COLORS
   col=col*Color[0]+Color[1];
#endif
#if DITHER
   ApplyDither(col.rgb, pixel.xy);
#endif
   return col;
}
/******************************************************************************/
VecH4 DrawTexCubic_PS(NOPERSP Vec2 inTex:TEXCOORD,
                      NOPERSP PIXEL              ):TARGET
{
   VecH4 col=TexCubicPlus(inTex);
#if COLORS
   col=col*Color[0]+Color[1];
#endif
#if DITHER
   ApplyDither(col.rgb, pixel.xy);
#endif
   return col;
}
VecH4 DrawTexCubicRGB_PS(NOPERSP Vec2 inTex:TEXCOORD,
                         NOPERSP PIXEL              ):TARGET
{
   VecH4 col=VecH4(TexCubicPlusRGB(inTex), 1);
#if COLORS
   col=col*Color[0]+Color[1];
#endif
#if DITHER
   ApplyDither(col.rgb, pixel.xy);
#endif
   return col;
}
/******************************************************************************/
