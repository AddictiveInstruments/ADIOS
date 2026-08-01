#include "render.h"

extern unsigned char speed;

// From RenderTabs.h
extern unsigned char multtable[];
extern unsigned char sinus[];
extern unsigned char rectangle[];

// From render.c
extern unsigned char pitches[256]; 
extern unsigned char sampledConsonantFlag[256]; // tab44800
extern unsigned char amplitude1[256];
extern unsigned char amplitude2[256];
extern unsigned char amplitude3[256];
extern unsigned char frequency1[256];
extern unsigned char frequency2[256];
extern unsigned char frequency3[256];

extern void Output(int index, unsigned char A);

static void CombineGlottalAndFormants(unsigned char phase1, unsigned char phase2, unsigned char phase3, unsigned char frm_idx)
{
  unsigned int tmp;
  
  tmp   = multtable[sinus[phase1]     | amplitude1[frm_idx]];
  tmp  += multtable[sinus[phase2]     | amplitude2[frm_idx]];
  tmp  += tmp > 255 ? 1 : 0; // if addition above overflows, we for some reason add one;
  tmp  += multtable[rectangle[phase3] | amplitude3[frm_idx]];
  tmp  += 136;
  //tmp >>= 4; // Scale down to 0..15 range of C64 audio.
  
  //Output(0, tmp & 0xf);
  Output(0, tmp);
}



// PROCESS THE FRAMES
//
// In traditional vocal synthesis, the glottal pulse drives filters, which
// are attenuated to the frequencies of the formants.
//
// SAM generates these formants directly with sin and rectangular waves.
// To simulate them being driven by the glottal pulse, the waveforms are
// reset at the beginning of each glottal pulse.
//
unsigned char ProcessFrame(unsigned char frm, unsigned char total_frm)
{
  unsigned char speedcounter = 72;
  unsigned char phase1 = 0;
  unsigned char phase2 = 0;
  unsigned char phase3 = 0;
  unsigned char mem66 = 0; //!! was not initialized
  
  //unsigned char frm_idx = 0;
  
  unsigned char glottal_pulse = pitches[frm];
  unsigned char mem38 = glottal_pulse - (glottal_pulse >> 2); // mem44 * 0.75
  
  //while(total_frm) {
    unsigned char flags = sampledConsonantFlag[frm];
    
    // unvoiced sampled phoneme?
    if(flags & 248) {
      RenderSample(&mem66, flags,frm);
      // skip ahead two in the phoneme buffer
      frm += 2;
      //total_frm -= 2;
      speedcounter = speed;
    } else {
      CombineGlottalAndFormants(phase1, phase2, phase3, frm);
      
      speedcounter--;
      if (speedcounter == 0) {
        frm++; //go to next amplitude
        // decrement the frame count
        //total_frm--;
        if(frm == total_frm) return frm;
        speedcounter = speed;
      }
      
      --glottal_pulse;
      
      if(glottal_pulse != 0) {
        // not finished with a glottal pulse
        
        --mem38;
        // within the first 75% of the glottal pulse?
        // is the count non-zero and the sampled flag is zero?
        if((mem38 != 0) || (flags == 0)) {
          // reset the phase of the formants to match the pulse
          phase1 += frequency1[frm];
          phase2 += frequency2[frm];
          phase3 += frequency3[frm];
          return frm;
        }
        
        // voiced sampled phonemes interleave the sample with the
        // glottal pulse. The sample flag is non-zero, so render
        // the sample for the phoneme.
        RenderSample(&mem66, flags, frm);
      }
    }
    
    glottal_pulse = pitches[frm];
    mem38 = glottal_pulse - (glottal_pulse>>2); // mem44 * 0.75
    
    // reset the formant wave generators to keep them in
    // sync with the glottal pulse
    phase1 = 0;
    phase2 = 0;
    phase3 = 0;
  //}
  return frm;
}



// PROCESS A FRAME
//
// In traditional vocal synthesis, the glottal pulse drives filters, which
// are attenuated to the frequencies of the formants.
//
// SAM generates these formants directly with sin and rectangular waves.
// To simulate them being driven by the glottal pulse, the waveforms are
// reset at the beginning of each glottal pulse.
//
void ProcessFrames(unsigned char total_frm)
{
  unsigned char speedcounter = 72;
  unsigned char phase1 = 0;
  unsigned char phase2 = 0;
  unsigned char phase3 = 0;
  unsigned char mem66 = 0; //!! was not initialized
  
  unsigned char frm_idx = 0;
  
  unsigned char glottal_pulse = pitches[0];
  unsigned char mem38 = glottal_pulse - (glottal_pulse >> 2); // mem44 * 0.75
  
  while(total_frm) {
    unsigned char flags = sampledConsonantFlag[frm_idx];
    
    // unvoiced sampled phoneme?
    if(flags & 248) {
      RenderSample(&mem66, flags,frm_idx);
      // skip ahead two in the phoneme buffer
      frm_idx += 2;
      total_frm -= 2;
      speedcounter = speed;
    } else {
      CombineGlottalAndFormants(phase1, phase2, phase3, frm_idx);
      
      speedcounter--;
      if (speedcounter == 0) {
        frm_idx++; //go to next amplitude
        // decrement the frame count
        total_frm--;
        if(total_frm == 0) return;
        speedcounter = speed;
      }
      
      --glottal_pulse;
      
      if(glottal_pulse != 0) {
        // not finished with a glottal pulse
        
        --mem38;
        // within the first 75% of the glottal pulse?
        // is the count non-zero and the sampled flag is zero?
        if((mem38 != 0) || (flags == 0)) {
          // reset the phase of the formants to match the pulse
          phase1 += frequency1[frm_idx];
          phase2 += frequency2[frm_idx];
          phase3 += frequency3[frm_idx];
          continue;
        }
        
        // voiced sampled phonemes interleave the sample with the
        // glottal pulse. The sample flag is non-zero, so render
        // the sample for the phoneme.
        RenderSample(&mem66, flags,frm_idx);
      }
    }
    
    glottal_pulse = pitches[frm_idx];
    mem38 = glottal_pulse - (glottal_pulse>>2); // mem44 * 0.75
    
    // reset the formant wave generators to keep them in
    // sync with the glottal pulse
    phase1 = 0;
    phase2 = 0;
    phase3 = 0;
  }
}
