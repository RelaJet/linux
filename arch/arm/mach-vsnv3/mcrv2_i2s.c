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

#define AFE_ADC_DIGITAL_GAIN AFE_ADC_DIG_GAIN
#define AFE_DAC_DIGITAL_GAIN AFE_DAC_DIG_GAIN


static struct workqueue_struct *mcrv2_i2s_workq = NULL;

static int mcrv2_i2s_set_dai_sysclk(struct snd_soc_dai *codec_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int mcrv2_i2s_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{

	return 0;
}

static int mcrv2_i2s_pcm_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{

	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_codec *codec = rtd->codec;
	struct vsnv3_afe_data *vsnv3_afe_data= codec->control_data;
	
	if(substream->stream==SNDRV_PCM_STREAM_CAPTURE) //capture
	{
	/*
		snd_soc_write(codec, AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN), vsnv3_afe_data->mic_digital_gain);
		MMPF_Audio_SetADCDigitalGain(vsnv3_afe_data->mic_digital_gain&0xFF); //remove gbADCDigitalGain or keep this 
	*/
	}
	else	//playback
	{
	/*
		if(!vsnv3_afe_data->mute){
			snd_soc_write(codec, AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN), vsnv3_afe_data->spk_digital_gain);
			MMPF_Audio_SetDACDigitalGain(vsnv3_afe_data->spk_digital_gain&0xFF);//remove gbADCDigitalGain or keep this 
		}
	*/
	}
	
	
	return 0;
}

//TODO: mute / unmute (roll back to previous volume setting)
static int mcrv2_i2s_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct vsnv3_afe_data *vsnv3_afe_data= codec->control_data;
	vsnv3_afe_data->mute = mute;
	if (mute) {
	/*	
		//mute SPK
		u16 spk_digital_gain = vsnv3_afe_data->spk_digital_gain;		
		snd_soc_write(codec, AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN), 0x0);
		MMPF_Audio_SetDACDigitalGain(vsnv3_afe_data->spk_digital_gain&0xFF);//remove gbADCDigitalGain or keep this 
		vsnv3_afe_data->spk_digital_gain = spk_digital_gain;
	*/	
	}
	else {
	/*
		snd_soc_write(codec, AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN), vsnv3_afe_data->spk_digital_gain);
		MMPF_Audio_SetDACDigitalGain(vsnv3_afe_data->spk_digital_gain&0xFF);//remove gbADCDigitalGain or keep this 
	*/
	}
	
	return 0;
}

static int mcrv2_i2s_set_bias_level(struct snd_soc_codec *codec,
	enum snd_soc_bias_level level)
{
	codec->dapm.bias_level = level;
	return 0;
}

#define VSNV3_AFE_RATES SNDRV_PCM_RATE_8000_48000// (SNDRV_PCM_RATE_8000|SNDRV_PCM_RATE_11025|SNDRV_PCM_RATE_16000 )

#define VSNV3_AFE_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

static struct snd_soc_dai_ops mcrv2_dai_ops = {
	.hw_params	= mcrv2_i2s_pcm_hw_params,
	.digital_mute	= mcrv2_i2s_mute,
	.set_fmt		= mcrv2_i2s_set_dai_fmt,
	.set_sysclk	= mcrv2_i2s_set_dai_sysclk,
};

static struct snd_soc_dai_driver mcrv2_i2s_dai = {
	.name = "mcrv2-i2s-codec-dai",
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
	.ops = &mcrv2_dai_ops,
};

static void mcrv2_i2s_work(struct work_struct *work)
{
	struct snd_soc_dapm_context *dapm =
		container_of(work, struct snd_soc_dapm_context,
			     delayed_work.work);
	struct snd_soc_codec *codec = dapm->codec;
	mcrv2_i2s_set_bias_level(codec, codec->dapm.bias_level);
}

static int mcrv2_i2s_suspend(struct snd_soc_codec *codec, pm_message_t state)
{
	mcrv2_i2s_set_bias_level(codec, SND_SOC_BIAS_OFF);
	return 0;
}

static int mcrv2_i2s_resume(struct snd_soc_codec *codec)
{
	mcrv2_i2s_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	if (codec->dapm.suspend_bias_level == SND_SOC_BIAS_ON) {
		codec->dapm.bias_level = SND_SOC_BIAS_ON;
		queue_delayed_work(mcrv2_i2s_workq , &codec->dapm.delayed_work,
			msecs_to_jiffies(1000));
	}

	return 0;
}

static const struct snd_kcontrol_new mcrv2_i2s_controls[] = {
	SOC_DOUBLE("Mic PGA Capture Volume", AFE_REG_OFFSET(AFE_ADC_ANA_LPGA_GAIN), 0, 8,31, 0),
	//SOC_DOUBLE("Mic Boost", AFE_REG_OFFSET(AFE_ADC_BOOST_CTL), 2, 0, 3, 0),
	SOC_DOUBLE("MIC Digital Gain", AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN), 0, 8,0xA1, 0),
	SOC_DOUBLE("SPK Digital Gain", AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN), 0, 8,0x57, 0)
};

static inline unsigned int mcrv2_i2s_read(struct snd_soc_codec *codec,
                                                  unsigned int reg)
{
	struct vsnv3_afe_data *vsnv3_afe_data= codec->control_data;
	
	unsigned int value;

	//TODO: read digital gain 
	//if( reg==AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN) )
	//	value = vsnv3_afe_data->mic_digital_gain;
	//else if(reg==AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN))
	//	value = vsnv3_afe_data->spk_digital_gain;
	//else
		value = readw(vsnv3_afe_data->base + reg);
	
	pr_debug( "%s: Reg[0x%x] = 0x%x \n",__FUNCTION__,(int)(vsnv3_afe_data->base + reg),value);
	return value;
}
  
static inline int mcrv2_i2s_write(struct snd_soc_codec *codec, unsigned int reg,
                         unsigned int value)
{
	struct vsnv3_afe_data *vsnv3_afe_data = codec->control_data;

	pr_debug( "%s: Reg[0x%x] = 0x%x \n",__FUNCTION__,(int)(vsnv3_afe_data->base + reg),value);  

	//TODO: write digital gain
	//if( reg==AFE_REG_OFFSET(AFE_ADC_DIGITAL_GAIN) )
	//	vsnv3_afe_data->mic_digital_gain = value;

	//if( reg==AFE_REG_OFFSET(AFE_DAC_DIGITAL_GAIN) )
	//	vsnv3_afe_data->spk_digital_gain = value;
	
	writew(value, vsnv3_afe_data->base + reg);

	return 0;
}

static int mcrv2_i2s_probe(struct snd_soc_codec *codec)
{
	struct vsnv3_afe_data *vsnv3_afe_data = codec->dev->platform_data;
	//u16 reg;

	vsnv3_afe_data->vsnv3afe.codec = codec;
	codec->control_data = vsnv3_afe_data;

	INIT_DELAYED_WORK(&codec->dapm.delayed_work, mcrv2_i2s_work );
	mcrv2_i2s_workq  = create_workqueue("mcrv2_i2s_work_q");
	if (mcrv2_i2s_workq  == NULL)
		return -ENOMEM;

	//TODO: Set default gain here 
	//MMPF_Audio_SetADCAnalogGain(0xF,0); 
	//MMPF_Audio_SetDACAnalogGain(0x0);	//0db

	/* Set controls */
	snd_soc_add_controls(codec, mcrv2_i2s_controls,
										ARRAY_SIZE(mcrv2_i2s_controls));

	/* Off, with power on */
	mcrv2_i2s_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	return 0;
}


/* power down chip */
static int mcrv2_i2s_remove(struct snd_soc_codec *codec)
{
	mcrv2_i2s_set_bias_level(codec, SND_SOC_BIAS_OFF);

	if (mcrv2_i2s_workq)
		destroy_workqueue(mcrv2_i2s_workq);
	return 0;
}


  
static struct snd_soc_codec_driver soc_codec_dev_afe = {
	 .read = mcrv2_i2s_read,
	.write = mcrv2_i2s_write,
	.set_bias_level = mcrv2_i2s_set_bias_level,
	.probe = mcrv2_i2s_probe,
 	.remove =mcrv2_i2s_remove,
 	.resume = mcrv2_i2s_resume,
 	.suspend = mcrv2_i2s_suspend
 
};

static int mcrv2_i2s_platform_probe(struct platform_device *pdev)
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

	vsnv3_afe_data->mic_digital_gain = 0x6464;
	vsnv3_afe_data->spk_digital_gain = 0x5656;
	vsnv3_afe_data->mute = 0;
	pdev->dev.platform_data = (void*)vsnv3_afe_data;
	return snd_soc_register_codec(&pdev->dev,
			&soc_codec_dev_afe, &mcrv2_i2s_dai, 1);

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

static int mcrv2_i2s_platform_remove(struct platform_device *pdev)
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


static struct platform_driver mcrv2_i2s_driver = {
	.driver = {
		.name = "mcrv2-i2s-codec",
		.owner = THIS_MODULE,
	},
	.probe = mcrv2_i2s_platform_probe,
	.remove =  mcrv2_i2s_platform_remove
};

module_platform_driver(mcrv2_i2s_driver)
	
MODULE_DESCRIPTION("AIT I2S audio codec");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
