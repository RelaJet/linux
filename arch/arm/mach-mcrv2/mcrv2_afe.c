//Vin:Todo
//GPL

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <linux/clk.h>

#include <mach/mmpf_typedef.h>
#include <mach/mmp_reg_audio.h>
#include <mach/mmp_reg_gbl.h>

#include <mach/mcrv2_afe.h>
#include "mmpf_mcrv2_audio_ctl.h"
#if defined(CONFIG_AIT_FAST_BOOT)
#include <mach/ait_alsa_ipc.h>
#endif

#define SUPPORT_MIC_NATIVE_CTRL     (1)

#define MCRV2_MICVOL_CTL_ID    (0xbabe)
    #define MCRV2_MIC_MIN_DB    (0 )
    #define MCRV2_MIC_DEF_DB    (12)
    #define MCRV2_MIC_MAX_DB    (50)

#define MCRV2_MICVOL_DGAIN     (0x4447) // 'DG'
    #define MCRV2_MIN_DGAIN_VAL (0) // mute
    #define MCRV2_MAX_DGAIN_VAL (161)
    #if defined (CONFIG_AIT_FAST_BOOT)
    #define MCRV2_DEF_DGAIN_VAL (81+25) // 19 dB
    #else
    #define MCRV2_DEF_DGAIN_VAL (81+9) // 12dB
    #endif
    #define MCRV2_MUTE_DGAIN_VAL (0)
    
#define MCRV2_MICVOL_AGAIN     (0x4147) // 'AG' 
    #define MCRV2_MIN_AGAIN_VAL (0 )    // 0 db
    #define MCRV2_MAX_AGAIN_VAL (31)    // 31 db  
    //#define MCRV2_DEF_AGAIN_VAL (0 )     // 0db
    #if defined (CONFIG_AIT_FAST_BOOT)
    #define MCRV2_DEF_AGAIN_VAL (MCRV2_MAX_AGAIN_VAL)    
    #else
    #define MCRV2_DEF_AGAIN_VAL (15)
    #endif
    
#define MCRV2_SPKVOL_CTL_ID    (0xdade)
    
    #define MCRV2_SPK_NOFF      (38) //normalize volume from 0.
    
    #define MCRV2_SPK_MIN_DB    (-38)
    #define MCRV2_SPK_DEF_DB    (0 )
    #define MCRV2_SPK_MAX_DB    (12 ) 
    
#define AFE_ADC_DIGITAL_GAIN AFE_ADC_DIG_GAIN
#define AFE_DAC_DIGITAL_GAIN AFE_DAC_DIG_GAIN

#define MCRV2_AUDIO_ENHANCE (0xaec0) 

#define MCRV2_AUDIO_IPC     (0xcafe)

static struct workqueue_struct *vsnv3_workq = NULL;
extern AUDIO_IPC_MODE audio_ipc_mode ;


#if SUPPORT_MIC_NATIVE_CTRL==0
static int mcrv2_mic_db2afegain(struct vsnv3_afe_data *afe, short voldb)
{
    MMP_UBYTE dig_gain,pga_gain;
    if(voldb >= 0 ) {
        if(voldb > 31 ) {
            pga_gain = 31 - ((voldb - 31) % 3 ? 3 - (voldb - 31) % 3 : 0) ;
            dig_gain = voldb - pga_gain ;
            dig_gain = (dig_gain * 4 /3) +  (ADC_DIG_GAIN_0DB >> 8);
        }
        else {
        	pga_gain = voldb ;
        	dig_gain = (ADC_DIG_GAIN_0DB >> 8 ) ;
        }
        afe->mic_digital_gain = dig_gain ;
        afe->mic_analog_gain = pga_gain;
        afe->mic_db = voldb ;
    }    
    else {
    	dbg_printf(3,"Adjust Db must > 0\r\n");
    }
	  //dbg_printf(3,"[sean]uac2adc : dig(0x%x),ana(0x%x),db:%d\r\n",afe->mic_digital_gain,afe->mic_analog_gain,afe->mic_db);
    return 0 ;
}
#endif

static int mcrv2_mic_set_gain(struct vsnv3_afe_data *afe)
{
  
    if(audio_ipc_mode==AUDIO_IPC_NONE) {
    //clk_enable(afe->clk);
#if SUPPORT_MIC_NATIVE_CTRL
    if(afe->mic_dgain_val==MCRV2_MUTE_DGAIN_VAL) {
        MMPF_Audio_SetADCMute(MMP_TRUE) ;
        dbg_printf(3,"mic_mute\n"); 
    }   
    else {
        dbg_printf(3,"mic_set(hw : analog : 0x%x,digital:0x%x)\n",afe->mic_again_val,afe->mic_dgain_val);
        MMPF_Audio_SetADCAnalogGain(afe->mic_again_val, MMP_FALSE);//gbADCAnalogGain
        MMPF_Audio_SetADCDigitalGain(afe->mic_dgain_val);//gbADCDigitalGain
        MMPF_Audio_SetADCMute(MMP_FALSE);
        
    } 
#else
    if(afe->mic_db==MCRV2_MIC_MIN_DB) { // implement as mute
        MMPF_Audio_SetADCMute(MMP_TRUE) ;
        dbg_printf(3,"mic_mute\n"); 
    }
    else {
        dbg_printf(3,"mic_set(%d)db (hw : analog : 0x%x,digital:0x%x)\n",afe->mic_db,afe->mic_analog_gain,afe->mic_digital_gain);
        MMPF_Audio_SetADCAnalogGain(afe->mic_analog_gain, MMP_FALSE);//gbADCAnalogGain
        MMPF_Audio_SetADCDigitalGain(afe->mic_digital_gain);//gbADCDigitalGain
        MMPF_Audio_SetADCMute(MMP_FALSE);
    }
#endif
    }
    else {
#if defined(CONFIG_AIT_FAST_BOOT)
 #if SUPPORT_MIC_NATIVE_CTRL
      if(afe->mic_dgain_val==MCRV2_MUTE_DGAIN_VAL) {
          alsa_ipc_mute(ALSA_MIC,1) ;
          dbg_printf(3,"ipc.mic_mute\n"); 
      }   
      else {
          dbg_printf(3,"ipc.mic_set(hw : analog : 0x%x,digital:0x%x)\n",afe->mic_again_val,afe->mic_dgain_val);
          alsa_ipc_setgain(ALSA_MIC,afe->mic_dgain_val,afe->mic_again_val); 
          alsa_ipc_mute(ALSA_MIC,0) ;         
      } 
  #else
      if(afe->mic_db==MCRV2_MIC_MIN_DB) { // implement as mute
          alsa_ipc_mute(ALSA_MIC,0) ;
          dbg_printf(3,"ipc.mic_unmute\n"); 
      }
      else {
          dbg_printf(3,"ipc.mic_set(%d)db (hw : analog : 0x%x,digital:0x%x)\n",afe->mic_db,afe->mic_analog_gain,afe->mic_digital_gain);
          alsa_ipc_setgain(ALSA_MIC,afe->mic_digital_gain,afe->mic_analog_gain);          
      }
  #endif
#endif
    }
    //clk_disable(afe->clk);
    return 0;   
}

static int mcrv2_spk_db2dacgain(struct vsnv3_afe_data *afe,short db)
{
    if(db > MCRV2_SPK_MAX_DB) {
        db = MCRV2_SPK_MAX_DB ;
    }
    afe->spk_db = db ;
    afe->spk_analog_gain = LOUT_DB2BITS(0) ; //alyways 0db.
    
    //02.27 same formula 12 ~ -15.5dB        
    if ( afe->spk_db > -16 ) {
        afe->spk_digital_gain = 2 * (afe->spk_db) + 63 ;
    }
    else {
        afe->spk_digital_gain = 47 + afe->spk_db ;
    }
    // TBD :here digital gain    
    //afe->spk_digital_gain = 
    return 0 ;
}

static int mcrv2_spk_set_gain(struct vsnv3_afe_data *afe)
{
    //clk_enable(afe->clk);
    if(audio_ipc_mode==AUDIO_IPC_NONE) {
      if(afe->spk_db==MCRV2_SPK_MIN_DB) {
        dbg_printf(0,"spk mute\r\n");  
        MMPF_Audio_SetDACMute();  
      }
      else {
        dbg_printf(3,"spk_set(%d)db( hw_d: 0x%x)\n",afe->spk_db,afe->spk_digital_gain);
        MMPF_Audio_SetDACDigitalGain(afe->spk_digital_gain) ;  
        dbg_printf(3,"spk_set(%d)db( hw_a: 0x%x)\n",afe->spk_db,afe->spk_analog_gain);
        MMPF_Audio_SetDACAnalogGain(afe->spk_analog_gain);  
      }
    }
    else {
#if defined(CONFIG_AIT_FAST_BOOT)
      if(afe->spk_db==MCRV2_SPK_MIN_DB) {
        alsa_ipc_mute(ALSA_SPK,1) ;
        //dbg_printf(3,"ipc.spk_mute\n"); 
      }
      else {
        //dbg_printf(3,"ipc.spk_set(%d)db (hw : analog : 0x%x,digital:0x%x)\n",afe->spk_db,afe->spk_analog_gain,afe->spk_digital_gain);
        alsa_ipc_setgain(ALSA_SPK,afe->spk_digital_gain,afe->spk_analog_gain);       
        // fixed mute -> unmute flow
        alsa_ipc_mute(ALSA_SPK,0) ;   
      }
#endif      
    }
    //clk_disable(afe->clk);	
    return 0;   
}

static int vsnv3_afe_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int vsnv3_afe_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{

	return 0;
}

static int vsnv3_afe_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct vsnv3_afe_data *vsnv3_afe_data= codec->control_data;
	if(substream->stream==SNDRV_PCM_STREAM_CAPTURE) //capture
	{
#if SUPPORT_MIC_NATIVE_CTRL
		snd_soc_write(codec, AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN), vsnv3_afe_data->mic_dgain_val);
		MMPF_Audio_SetADCDigitalGain(vsnv3_afe_data->mic_dgain_val&0xFF); //remove gbADCDigitalGain or keep this

#else	    
		snd_soc_write(codec, AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN), vsnv3_afe_data->mic_digital_gain);
		MMPF_Audio_SetADCDigitalGain(vsnv3_afe_data->mic_digital_gain&0xFF); //remove gbADCDigitalGain or keep this
#endif
		
	}
	else	//playback
	{
		if(!vsnv3_afe_data->mute){
			snd_soc_write(codec, AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN), vsnv3_afe_data->spk_digital_gain);
			MMPF_Audio_SetDACDigitalGain(vsnv3_afe_data->spk_digital_gain&0xFF);//remove gbADCDigitalGain or keep this 
		}
	}
	
	
	return 0;
}

//TODO: mute / unmute (roll back to previous volume setting)
static int vsnv3_afe_mute(struct snd_soc_dai *dai, int mute)
{
  struct snd_soc_codec *codec = dai->codec;
  struct vsnv3_afe_data *vsnv3_afe_data= codec->control_data;
  //vsnv3_afe_data->mute = mute;
  if(mute) {
    MMPF_Audio_SetDACMute();
  } else {    
    if(!vsnv3_afe_data->mute) {
      mcrv2_spk_set_gain(vsnv3_afe_data);
    }
  }
	return 0;
}

static int vsnv3_afe_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
#if 0
	u16 bias = snd_soc_read(codec, AFE_REG_OFFSET(AFE_ADC_CTL_REG4))&0xff00;//, ADC_MIC_BIAS_ON | ADC_MIC_BIAS_VOLTAGE075AVDD);

	switch (level) {
	case SND_SOC_BIAS_ON:
		/* set vmid to 50k and unmute dac */
		bias|=ADC_MIC_BIAS_ON | ADC_MIC_BIAS_VOLTAGE075AVDD;
		break;
	case SND_SOC_BIAS_PREPARE:
		bias|=ADC_MIC_BIAS_ON | ADC_MIC_BIAS_VOLTAGE090AVDD;	
		break;
	case SND_SOC_BIAS_STANDBY:
		if (codec->dapm.bias_level == SND_SOC_BIAS_OFF)
			snd_soc_cache_sync(codec);

		/* mute dac and set vmid to 500k, enable VREF */
		bias|=ADC_MIC_BIAS_ON | ADC_MIC_BIAS_VOLTAGE065AVDD;
	
		break;
	case SND_SOC_BIAS_OFF:
		bias|=ADC_MIC_BIAS_OFF;
		break;
	}

	snd_soc_write(codec, AFE_REG_OFFSET(AFE_ADC_CTL_REG4), bias);

#endif

	codec->dapm.bias_level = level;

	return 0;
}

#define VSNV3_AFE_RATES SNDRV_PCM_RATE_8000_48000// (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025|SNDRV_PCM_RATE_16000 )

#define VSNV3_AFE_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

static struct snd_soc_dai_ops wm8971_dai_ops = {
	.hw_params	= vsnv3_afe_pcm_hw_params,
	.digital_mute	= vsnv3_afe_mute,
	.set_fmt	= vsnv3_afe_set_dai_fmt,
	.set_sysclk	= vsnv3_afe_set_dai_sysclk,
};

static struct snd_soc_dai_driver vsnv3_afe_dai = {
	.name = "vsnv3-afe-hifi",
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = VSNV3_AFE_RATES,
		.formats = VSNV3_AFE_FORMATS,},
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = VSNV3_AFE_RATES,
		.formats = VSNV3_AFE_FORMATS,},
	.ops = &wm8971_dai_ops,
};

static void vsnv3_afe_work(struct work_struct *work)
{
	struct snd_soc_dapm_context *dapm =
		container_of(work, struct snd_soc_dapm_context,
			     delayed_work.work);
	struct snd_soc_codec *codec = dapm->codec;
	vsnv3_afe_set_bias_level(codec, codec->dapm.bias_level);
}

static int vsnv3_afe_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	vsnv3_afe_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int vsnv3_afe_resume(struct snd_soc_codec *codec)
{
	vsnv3_afe_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	if (codec->dapm.suspend_bias_level == SND_SOC_BIAS_ON) {
		codec->dapm.bias_level = SND_SOC_BIAS_ON;
		queue_delayed_work(vsnv3_workq , &codec->dapm.delayed_work,
			msecs_to_jiffies(1000));
	}

	return 0;
}

static const char *ait_audio_enh[] = {"NONE","AEC" ,"AEC-DBG"};
static const char *ait_audio_ipc[] = {"NONE","IPC" };
  
static const struct soc_enum ait_afe_enum[] = 
{
	SOC_ENUM_SINGLE(MCRV2_AUDIO_ENHANCE,0,2+1/*3*/,ait_audio_enh),
	SOC_ENUM_SINGLE(MCRV2_AUDIO_IPC,0,2,ait_audio_ipc),
};

static const struct snd_kcontrol_new vsnv3_afe_controls[] = {
	//SOC_DOUBLE("Mic PGA Capture Volume", AFE_REG_OFFSET(AFE_ADC_ANA_LPGA_GAIN), 0, 8,31, 0),
#if SUPPORT_MIC_NATIVE_CTRL	
	SOC_DOUBLE("Mic PGA"  , MCRV2_MICVOL_AGAIN, 0, 0,MCRV2_MAX_AGAIN_VAL, 0),
	SOC_DOUBLE("Mic DGain", MCRV2_MICVOL_DGAIN, 0, 0,MCRV2_MAX_DGAIN_VAL, 0),
#else
	SOC_DOUBLE("Mic Vol", MCRV2_MICVOL_CTL_ID, 0, 0,MCRV2_MIC_MAX_DB, 0),
#endif	
	//SOC_DOUBLE("AFE Digital Gain", AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN), 0, 8,0xA1, 0),
	//SOC_DOUBLE("SPK Digital Gain", AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN), 0, 8,0x57, 0)
	SOC_DOUBLE("Spk Vol", MCRV2_SPKVOL_CTL_ID, 0, 0,(MCRV2_SPK_MAX_DB+MCRV2_SPK_NOFF), 0),
	SOC_ENUM("Aud Enh", ait_afe_enum[0]),
#if defined(CONFIG_AIT_FAST_BOOT)
	SOC_ENUM("Aud IPC", ait_afe_enum[1]),
#endif
};

AUDIO_ENH_MODE g_audio_mode = AUDIO_ENH_NONE;   
int g_aec_dbg = 0 ;
EXPORT_SYMBOL(g_audio_mode);
EXPORT_SYMBOL(g_aec_dbg);
//EXPORT_SYMBOL(audio_ipc_mode) ;

static inline unsigned int vsnv3_afe_read(struct snd_soc_codec *codec,
                                                  unsigned int reg)
{
	struct vsnv3_afe_data *vsnv3_afe_data= codec->control_data;
	
	unsigned int value;
    //clk_enable(vsnv3_afe_data->clk);
	if( reg==AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN) )
#if SUPPORT_MIC_NATIVE_CTRL
		value = vsnv3_afe_data->mic_dgain_val;
#else	    
		value = vsnv3_afe_data->mic_digital_gain;
#endif		
	else if(reg==AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN))
		value = vsnv3_afe_data->spk_digital_gain;
	else if (reg==MCRV2_MICVOL_CTL_ID) {
	  value = vsnv3_afe_data->mic_db;
	  //printk(KERN_ERR"afe_read:mic_db : %d\r\n",  value);
	}
	else if (reg==MCRV2_SPKVOL_CTL_ID) {
	  value = vsnv3_afe_data->spk_db + MCRV2_SPK_NOFF ;    
	  //printk(KERN_ERR"afe_read:spk_db : %d\r\n",  value);
	}
	else if (reg==MCRV2_MICVOL_DGAIN) {
	  value = vsnv3_afe_data->mic_dgain_val ;    
  }
  else if (reg==MCRV2_MICVOL_AGAIN) {
    value = vsnv3_afe_data->mic_again_val ;
  }
	else if(reg == MCRV2_AUDIO_ENHANCE){
		if(g_audio_mode==AUDIO_ENH_AEC) {
			value = (g_aec_dbg) ? AUDIO_ENH_AEC_DBG:AUDIO_ENH_AEC;	
		}
		else {
			value = g_audio_mode;
		}
	}
	else if (reg == MCRV2_AUDIO_IPC) {
	  value = audio_ipc_mode ;  
	}
	else {
	    value = readw(vsnv3_afe_data->base + reg);
	}
	pr_debug( "%s: Reg[0x%x] = 0x%x \n",__FUNCTION__,(int)(vsnv3_afe_data->base + reg),value);
	return value;
}
  
static inline int vsnv3_afe_write(struct snd_soc_codec *codec, unsigned int reg,
                         unsigned int value)
{
	struct vsnv3_afe_data *vsnv3_afe_data = codec->control_data;

	
	pr_debug( "%s: Reg[0x%x] = 0x%x \n",__FUNCTION__,(int)(vsnv3_afe_data->base + reg),value);  

	if( reg==AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN) )
#if SUPPORT_MIC_NATIVE_CTRL
		vsnv3_afe_data->mic_dgain_val    = value;
#else	    
		vsnv3_afe_data->mic_digital_gain = value;
#endif
	if( reg==AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN) )
		vsnv3_afe_data->spk_digital_gain = value;
#if SUPPORT_MIC_NATIVE_CTRL==0	
	if( reg == MCRV2_MICVOL_CTL_ID) {   
    		mcrv2_mic_db2afegain(vsnv3_afe_data,value) ;
    		mcrv2_mic_set_gain(vsnv3_afe_data);
    		//return 0 ;    
		goto exit;
  	}
#else
  	if( reg == MCRV2_MICVOL_DGAIN) {
    		vsnv3_afe_data->mic_dgain_val    = value;
    		mcrv2_mic_set_gain(vsnv3_afe_data);
    		//return 0 ;  
    		goto exit;
  	}

	if( reg == MCRV2_MICVOL_AGAIN) {
		vsnv3_afe_data->mic_again_val    = value;
		mcrv2_mic_set_gain(vsnv3_afe_data); 
		//return 0 ; 
		goto exit;
	}

#endif    
	if(reg == MCRV2_SPKVOL_CTL_ID) {
		mcrv2_spk_db2dacgain(vsnv3_afe_data,value-MCRV2_SPK_NOFF); 
		mcrv2_spk_set_gain(vsnv3_afe_data) ;
		//return 0;
		goto exit;
	}

	if(reg == MCRV2_AUDIO_ENHANCE){
		if(value==AUDIO_ENH_AEC_DBG) {
			g_aec_dbg=1 ;
#if defined(CONFIG_AIT_FAST_BOOT)
      g_audio_mode = AUDIO_ENH_AEC_DBG ;
#else			
			g_audio_mode = AUDIO_ENH_AEC ;
#endif			
		}
		else {
			g_audio_mode = value;
			g_aec_dbg = 0 ;
		}
		pr_info("Aud Enh = %d\n",g_audio_mode);
		
#if defined(CONFIG_AIT_FAST_BOOT)
    alsa_ipc_set_aec(g_audio_mode);
#endif		
		
		goto exit;
	}
	else if (reg == MCRV2_AUDIO_IPC ){
#if defined(CONFIG_AIT_FAST_BOOT)

	  int ret ;
	  //audio_ipc_mode = value ;
	  pr_info("Aud IPC = %d\n", value);
	  if(value==AUDIO_IPC_EN) {
	    ret = alsa_ipc_setowner(ALSA_MIC,AFE_OWNER_IS_RTOS);  
	    ret = alsa_ipc_setowner(ALSA_SPK,AFE_OWNER_IS_RTOS);  
	  }
	  else {
	    ret =alsa_ipc_setowner(ALSA_MIC,AFE_OWNER_IS_LINUX);  
	    ret =alsa_ipc_setowner(ALSA_SPK,AFE_OWNER_IS_LINUX);  
	  }
	  
	  if( (alsa_ipc_getowner(ALSA_MIC)==AFE_OWNER_IS_LINUX) || ( alsa_ipc_getowner(ALSA_SPK)==AFE_OWNER_IS_LINUX ) ) {
	    audio_ipc_mode = AUDIO_IPC_NONE ;  
	  }
	  else {
	    audio_ipc_mode = AUDIO_IPC_EN ;
	  }
	  pr_info("#switch afe owner ret:%d\n",audio_ipc_mode);
	  goto exit ;
#endif
	}
	//if( !vsnv3_afe_data->mute || reg!=AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN) )
	if(audio_ipc_mode==AUDIO_IPC_NONE) {
	  writew(value, vsnv3_afe_data->base + reg);
  }
  else {
    pr_info("todo : ipc writew ,reg:0x%08x = 0x%04x\n",vsnv3_afe_data->base + reg, value);  
  }

exit:	
	//clk_disable(vsnv3_afe_data->clk);
	return 0;
}

static int vsnv3_afe_probe(struct snd_soc_codec *codec)
{
	struct vsnv3_afe_data *vsnv3_afe_data = codec->dev->platform_data;
	//u16 reg;

	vsnv3_afe_data->vsnv3afe.codec = codec;
	codec->control_data = vsnv3_afe_data;

	INIT_DELAYED_WORK(&codec->dapm.delayed_work, vsnv3_afe_work );
	vsnv3_workq  = create_workqueue("vsnv3afe");
	if (vsnv3_workq  == NULL)
		return -ENOMEM;

	//clk_enable(vsnv3_afe_data->clk);

	//MMPF_Audio_ResetAfeFifo();

	//MMPF_Audio_SetADCAnalogGain(vsnv3_afe_data->mic_digital_gain&0xFF, 0);

	
	//snd_soc_write(codec, AFE_REG_OFFSET(AFE_ADC_ANA_LPGA_GAIN), 0x1f1f);	//Init Audio and Analog gain

  //  mcrv2_mic_db2afegain(vsnv3_afe_data,value) ;
  //  mcrv2_mic_set_gain(vsnv3_afe_data);
 /*
	MMPF_Audio_SetADCAnalogGain(0xF,0); 
	MMPF_Audio_SetDACAnalogGain(0x0);	//0db
	*/
	
	//snd_soc_write(codec, AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN), vsnv3_afe_data->mic_digital_gain);	//Init Audio Digital gain: 0dB
	//snd_soc_write(codec, AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN), vsnv3_afe_data->spk_digital_gain);	//Init Audio Digital gain: 0dB	
	//clk_disable(vsnv3_afe_data->clk);

  mcrv2_mic_set_gain(vsnv3_afe_data);
  mcrv2_spk_set_gain(vsnv3_afe_data);
	/* Set controls */
	snd_soc_add_controls(codec, vsnv3_afe_controls,
										ARRAY_SIZE(vsnv3_afe_controls));

	/* Off, with power on */
	vsnv3_afe_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}


/* power down chip */
static int vsnv3_afe_remove(struct snd_soc_codec *codec)
{
	vsnv3_afe_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if (vsnv3_workq)
		destroy_workqueue(vsnv3_workq);
	return 0;
}


  
static struct snd_soc_codec_driver soc_codec_dev_afe = {
	 .read = vsnv3_afe_read,
	.write = vsnv3_afe_write,
	.set_bias_level = vsnv3_afe_set_bias_level,
	.probe = vsnv3_afe_probe,
 	.remove =vsnv3_afe_remove,
 	.resume = vsnv3_afe_resume,
 	.suspend = vsnv3_afe_suspend
 
};

static int vsnv3_afe_platform_probe(struct platform_device *pdev)
{
	struct resource *res, *mem;
	int ret;

	struct vsnv3_afe_data *vsnv3_afe_data; 
	pr_debug( "%s: %d \n",__FUNCTION__,__LINE__);

	vsnv3_afe_data = kzalloc(sizeof(struct vsnv3_afe_data), GFP_KERNEL);
	if (!vsnv3_afe_data) {
		dev_err(&pdev->dev,
			    "could not allocate memory for private data\n");
		return -ENOMEM;
	}

	vsnv3_afe_data->clk = clk_get(&pdev->dev, "afe_clk");
	if (IS_ERR(vsnv3_afe_data->clk)) {
		dev_err(&pdev->dev,
			    "could not get the clock for voice codec\n");
		ret = -ENODEV;
		goto fail1;
	}
//	clk_enable(vsnv3_afe_data->clk);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no mem resource\n");
		ret = -ENODEV;
		goto fail2;
	}

	vsnv3_afe_data->pbase = res->start;
	vsnv3_afe_data->base_size = resource_size(res);

	mem = request_mem_region(vsnv3_afe_data->pbase, vsnv3_afe_data->base_size,
				 pdev->name);
	if (!mem) {
		dev_err(&pdev->dev, "VCIF region already claimed\n");
		ret = -EBUSY;
		goto fail2;
	}

	vsnv3_afe_data->base = ioremap(vsnv3_afe_data->pbase, vsnv3_afe_data->base_size);
	if (!vsnv3_afe_data->base) {
		dev_err(&pdev->dev, "can't ioremap mem resource.\n");
		ret = -ENOMEM;
		goto fail3;
	}

  /*
	vsnv3_afe_data->mic_digital_gain = 0x8282;
	vsnv3_afe_data->spk_digital_gain = 0x5656;  
	vsnv3_afe_data->mic_db = 42 ;
	*/
	#if SUPPORT_MIC_NATIVE_CTRL
	vsnv3_afe_data->mic_dgain_val = MCRV2_DEF_DGAIN_VAL ;
	vsnv3_afe_data->mic_again_val = MCRV2_DEF_AGAIN_VAL ;
	#else
	mcrv2_mic_db2afegain(vsnv3_afe_data,MCRV2_MIC_DEF_DB);
	#endif
	mcrv2_spk_db2dacgain(vsnv3_afe_data,MCRV2_SPK_DEF_DB);
	
	vsnv3_afe_data->mute = 0;
	
	pdev->dev.platform_data = (void*)vsnv3_afe_data;
	return snd_soc_register_codec(&pdev->dev,
			&soc_codec_dev_afe, &vsnv3_afe_dai, 1);

//fail4:
//	iounmap(vsnv3_afe_data->base);
fail3:
	release_mem_region(vsnv3_afe_data->pbase, vsnv3_afe_data->base_size);
fail2:
	clk_disable(vsnv3_afe_data->clk);
	clk_put(vsnv3_afe_data->clk);
	vsnv3_afe_data->clk = NULL;
fail1:
	kfree(vsnv3_afe_data);
	return ret;	
}

static int vsnv3_afe_platform_remove(struct platform_device *pdev)
{

	struct vsnv3_afe_data *vsnv3_afe_data = platform_get_drvdata(pdev);
	printk("%s\r\n",__FUNCTION__);
	snd_soc_unregister_codec(&pdev->dev);

	iounmap(vsnv3_afe_data->base);
	release_mem_region(vsnv3_afe_data->pbase, vsnv3_afe_data->base_size);

	clk_disable(vsnv3_afe_data->clk);
	clk_put(vsnv3_afe_data->clk);
	vsnv3_afe_data->clk = NULL;

	kfree(vsnv3_afe_data);

	return 0;
}


static struct platform_driver vsnv3_afe_driver = {
	.driver = {
		.name = "vsnv3-afe-codec",
		.owner = THIS_MODULE,
	},
	.probe = vsnv3_afe_platform_probe,
	.remove =  vsnv3_afe_platform_remove
};

module_platform_driver(vsnv3_afe_driver)
	
MODULE_DESCRIPTION("AIT AFE audio driver");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
