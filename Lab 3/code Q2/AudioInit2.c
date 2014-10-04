/*****************************************************************
 * CS4281.c   the driver for soundcard cs4281 in VxWorks         *
 *                                                               *
 *          U of Colorado at Boulder                             *
 *****************************************************************/

/* VxWorks API includes */
#include "vxWorks.h"
#include "stdio.h"
#include "stdlib.h"
#include "ioLib.h"
#include "semLib.h"
#include "intLib.h"
#include "iv.h"
#include "sockLib.h"
#include "inetLib.h"
#include "stdioLib.h"
#include "strLib.h"
#include "hostLib.h"
#include "taskLib.h"
#include "fioLib.h"
#include "netinet/tcp.h"
/*#include "fft_header.h"*/

#include "tcpExample.h"

/* VxWorks 5.4 PCI driver interface includes */
#include "drv/pci/pciConfigLib.h"
#include "drv/pci/pciConfigShow.h"
#include "drv/pci/pciHeaderDefs.h"
#include "drv/pci/pciLocalBus.h"
#include "drv/pci/pciIntLib.h"


/* pcPentium BSP includes */
#include "sysLib.h"    

/* Cystal cs4281 and ac97 hardware */
#include "cs4281Registers.h"

/*VOID RecvTask(int sFd);*/
#define SERVER_STACK_SIZE 10000 /* stack size of server's work task */
/* Dac and Adc buffers */
#define DAC_BUFFER_SIZE   8192
#define ADC_BUFFER_SIZE   DAC_BUFFER_SIZE

extern unsigned char pbdata[];
extern const int PB_SIZE;
extern unsigned char bugs[];
extern const int BUG_SIZE;
char adNauseam[DAC_BUFFER_SIZE];
unsigned char saved_samples[DAC_BUFFER_SIZE];
/*static fd_set files;*/ 
UINT32 * tmp_int;

/* Local definition of CS4281 values */
#define PCI_VENDOR_ID_CIRRUS         0x1013
#define PCI_DEVICE_ID_CRYSTAL_CS4281  0x6005 
#define CS4281_pBA0       0xfc520000
#define CS4281_pBA1       0xfc510000
#define INT_NUM_IRQ0      0x20

static UINT32 j=0;


void *DAC_BUFFER = NULL;
void *ADC_BUFFER = NULL;
int PIPE_SNDBUF = ERROR;

/* Interrupt Service Related*/
unsigned char cs4281_irq;
SEM_ID  SEM_DMA_Playback, SEM_MY_Playback, SEM_DMA_Record,SEM_Sample; /* semophores */
int     CNT_DMA_Playback, CNT_DMA_Record; /* debug counter */
int     DTC_DMA_Playback, DTC_DMA_Record; /* Empty or Half Empty */
int flag1=1;




/*---------------------------------------------------------------------------
  Hardware interface For the CS4281
  --------------------------------------------------------------------------*/
/* Read from PCI Address 'offset'  */
UINT32 readl(UINT32 offset)
{
    UINT32 result;
    PCI_READ(offset, 0x0, &result);
    return result;
}
/* Write value to PCI Address 'offset' */
int    writel(UINT32 value, UINT32 offset)
{
    PCI_WRITE(offset,0x0, value);
    return 0;
}

/***************************************************************************
 cs4281_read_ac97: read a word from the CS4281 address space (based on BA0)
     step-1: write ACCAD (Commond Address Register) 46C h
     step-2: write ACCDA (Command Data    Register) 470 h, 0 for read
     step-3: write ACCTL (Control         Register) 460 h, initiating op
     step-4: read  ACCTL , until DCV is reset and [460h] = 17h
     step-5: if DCV not cleared, error
     step-6: read  ACSTS (Status          Register) 464 h, check VSTS bit
****************************************************************************/
int cs4281_read_ac97(UINT32 offset, UINT32 *value)
{
    UINT32 count, status;

    /* Make sure there is no data in ACSDA[47Ch] */
    status = readl(CS4281_pBA0 + BA0_ACSDA);

    /* Get the actual offset, and read ... */
    writel( offset - BA0_AC97_RESET, CS4281_pBA0 + BA0_ACCAD );
    writel( 0, CS4281_pBA0 + BA0_ACCDA );
    writel( ACCTL_DCV | ACCTL_CRW | ACCTL_VFRM | ACCTL_ESYN,
	    CS4281_pBA0 + BA0_ACCTL );

    /* Wait fo the read to occur */
    for( count = 0; count < 10; count ++ ) {
	taskDelay(25);
	/*udelay(25);*/

	/* check if read is complete */
	if(! (readl(CS4281_pBA0 + BA0_ACCTL) & ACCTL_DCV) )
	    break;
    }
    if( readl(CS4281_pBA0 + BA0_ACCTL) & ACCTL_DCV )
	return 1;

    /* Wait for the valid status bit to go active */
    for( count = 0; count < 10; count ++ ) {
	status = readl(CS4281_pBA0 + BA0_ACSTS);

	if(status & ACSTS_VSTS)
	    break;

	taskDelay(25);
	/*udelay(25);*/
    }
    if(!(status & ACSTS_VSTS))
	return 1;

    /* Read data from the AC97 register 474h */
    *value = readl( CS4281_pBA0 + BA0_ACSDA );

    return 0;
}

/****************************************************************************
  cs4281_wrote_ac97() : write a word to the cs4281 address space
      step-1: write ACCAD (Command Address Register) 46C h
      step-2: write ACCDA (Command Data    Register) 470 h
      step-3: write ACCTL (Control         Register) 460 h
      step-4: read  ACCTL,  DCV should be reset and [460h] = 07h
      step-5: if DCV not cleared, error
*****************************************************************************/
int cs4281_write_ac97(UINT32 offset, UINT32 value )
{
    UINT32 count, status;

    /* write to the actual AC97 register */
    writel(offset - BA0_AC97_RESET, CS4281_pBA0 + BA0_ACCAD);
    writel(value, CS4281_pBA0 + BA0_ACCDA);
    writel(ACCTL_DCV | ACCTL_VFRM |ACCTL_ESYN, CS4281_pBA0 + BA0_ACCTL);

    /* Wait for write to finish ... */
    for(count=0; count<10; count ++) {
	taskDelay(25);
	/*udelay(25);*/

	/* check if write complete */
	status = readl(CS4281_pBA0 + BA0_ACCTL);
	if(!(status & ACCTL_DCV))
	    break;
    }
    if(status & ACCTL_DCV)
	return 1;

    return 0;
}

/***************************************************************************
 cs4281_hw_init:   bring up the part
**************************************************************************/
int cs4281_hw_init( void )
{
    UINT32 ac97_slotid;
    UINT32 temp1, temp2;

    /****************************************************
     *      set up the Sound System configuration
     ****************************************************/
    printf("\nCS4281 HardWare Initialization ...\n");

    /* ease the 'write protect' */
    writel(0x4281, CS4281_pBA0 + BA0_CWPR);

    /* Blast the clock control register to 0, so that PLL starts out
       Blast the master serial port cntl register to 0, so that serial port
       starts out
    */
    writel(0, CS4281_pBA0 + BA0_CLKCR1);
    writel(0, CS4281_pBA0 + BA0_SERMC);

    /***** <1> Make ESYN go to 0, to turn off the Sync pulse */
    writel(0, CS4281_pBA0 + BA0_ACCTL);
    taskDelay(50);
    /*udelay(50);*/

    /***** <2> Drive ARST# pin low for 1uS, then drive high, so that the
	   external logic are reset
    */
    writel(0, CS4281_pBA0 + BA0_SPMC);
    taskDelay(100);
    /* udelay(100);*/
    writel(SPMC_RSTN, CS4281_pBA0 + BA0_SPMC);
    taskDelay(500);
    /*delayus(50000);*/

    /***** <3> Turn on the Sound System clocks */
    writel(CLKCR1_PLLP, CS4281_pBA0 + BA0_CLKCR1);
    taskDelay(500);
    /*delayus(50000);*/
    writel(CLKCR1_PLLP | CLKCR1_SWCE, CS4281_pBA0 + BA0_CLKCR1);

    /***** <4> Power on everything for now */
    writel(0x7e, CS4281_pBA0 + BA0_SSPM);

    /***** <5> Wait for clock stabilization */
    for(temp1=0; temp1<10000; temp1++) {
	taskDelay(10);
	/*udelay(1000);*/
	if( readl(CS4281_pBA0 + BA0_CLKCR1) & CLKCR1_DLLRDY )
	    break;
    }
    if(!(readl(CS4281_pBA0 + BA0_CLKCR1) & CLKCR1_DLLRDY)) {
	printf("cs4281: DLLRDY failed! \n");
	return -1;
    }

    /***** <6> Enable ASYNC generation */
    writel(ACCTL_ESYN, CS4281_pBA0 +BA0_ACCTL);

    /* wait for a while to start generating bit clock */
    taskDelay(500);
    /*dealyus(50000);*/

    /* Set the serial port timing configuration */
    writel( SERMC_PTC_AC97, CS4281_pBA0 + BA0_SERMC );

    /***** <7> Wait for the codec ready signal from the AC97 codec */
    for(temp1=0; temp1<1000; temp1++) {
	taskDelay(1);
	/*udelay(1000);*/
	if(readl(CS4281_pBA0 + BA0_ACSTS) & ACSTS_CRDY )
	    break;
    }
    if(!(readl(CS4281_pBA0 + BA0_ACSTS)& ACSTS_CRDY)) {
	printf("cs4281: ACTST never came ready!\n");
	return -1;
    }

    /***** <8> Assert the 'valid frame' signal to begin sending
	   commands to AC97 codec */
    writel(ACCTL_VFRM |ACCTL_ESYN, CS4281_pBA0 + BA0_ACCTL);

    /***** <9> Wait until CODEC calibration is finished.*/
    for(temp1=0; temp1<1000; temp1++) {
	taskDelay(10);
	/*	delayus(10000);*/
	if(cs4281_read_ac97(BA0_AC97_POWERDOWN, &temp2) )
	  return -1;
	if( (temp2 & 0x0000000F) == 0x0000000F )
	    break;
    }
    if( (temp2 & 0x0000000F) != 0x0000000F ) {
	printf("cs4281: Codec failed to calibrate\n");
	return -1;
    }

    /***** <12> Start digital data transfer of audio data to codec */
    writel(ACOSV_SLV3 | ACOSV_SLV4, CS4281_pBA0 + BA0_ACOSV );
    writel(ACISV_ISV3 | ACISV_ISV4, CS4281_pBA0 + BA0_ACISV );



     /************************************************
     *    Unmute the Master and
     *    Alternate (headphone) volumes, to max.
     ************************************************/
    cs4281_write_ac97(BA0_AC97_MASTER_VOLUME, 0x0000);
    cs4281_write_ac97(BA0_AC97_HEADPHONE_VOLUME, 0x0000);
    cs4281_write_ac97(BA0_AC97_MASTER_VOLUME_MONO, 0x0010);
    cs4281_write_ac97(BA0_AC97_PC_BEEP_VOLUME, 0x0010);
    cs4281_write_ac97(BA0_AC97_PHONE_VOLUME, 0x0008);
    cs4281_write_ac97(BA0_AC97_MIC_VOLUME, 0x0008);

    cs4281_write_ac97(BA0_AC97_CD_VOLUME, 0x0808);
    cs4281_write_ac97(BA0_AC97_VIDEO_VOLUME, 0x0808);
    cs4281_write_ac97(BA0_AC97_AUX_VOLUME, 0x0808);
    cs4281_write_ac97(BA0_AC97_PCM_OUT_VOLUME, 0x0000);

    cs4281_write_ac97(BA0_AC97_RECORD_GAIN, 0x0b0b);
    cs4281_write_ac97(BA0_AC97_RECORD_GAIN_MIC, 0x0b0b);

    
    /************************************************
     *    POWER on the DAC
     ************************************************/
    cs4281_read_ac97(BA0_AC97_POWERDOWN, &temp1);
    cs4281_write_ac97(BA0_AC97_POWERDOWN, temp1 &= 0xfdff);
    /* Wait until we sample a DAC ready state */
    for(temp2=0; temp2<32; temp2++) {
	taskDelay(1);
	/*	delayus(1000);*/
	cs4281_read_ac97(BA0_AC97_POWERDOWN, &temp1);
	if(temp1 & 0x2)
	    break;
    }

     /************************************************
     *    POWER on the ADC
     ************************************************/
    cs4281_read_ac97(BA0_AC97_POWERDOWN, &temp1);
    cs4281_write_ac97(BA0_AC97_POWERDOWN, temp1 &= 0xfeff);
    /* Wait until we sample a DAC ready state */
    for(temp2=0; temp2<32; temp2++) {
	taskDelay(1);
	/*	delayus(1000);*/
	cs4281_read_ac97(BA0_AC97_POWERDOWN, &temp1);
	if(temp1 & 0x1)
	    break;
    }

       /*
        For playback, we map AC97 slot 3 and 4(Left
        & Right PCM playback) to DMA Channel 0.
        Set the fifo to be 15 bytes at offset zero.
       */
       ac97_slotid = 0x01000f00;   
       writel(ac97_slotid, CS4281_pBA0 + BA0_FCR0);                 
       writel(ac97_slotid | FCRn_FEN, CS4281_pBA0 + BA0_FCR0);      

       /*
        For record, we map AC97 slots 3 and 4(Left
        & Right Record)	to DMA Channel 1.  Set the 
		fifo to be 15 bytes at offset 63.
       */
       ac97_slotid = 0x0b0a0f3f;   
       writel(ac97_slotid, CS4281_pBA0 + BA0_FCR1);                 
       writel(ac97_slotid | FCRn_FEN, CS4281_pBA0 + BA0_FCR1);      

       /*
         Map the Playback SRC to the same AC97 slots(3 & 4--
         --Playback left & right)as DMA channel 0.
         Map the record SRC to the same AC97 slots(10 & 11--
         -- Record left & right) as DMA channel 1.
       */
       ac97_slotid = 0x0b0a0100;      
       writel(ac97_slotid, CS4281_pBA0 + BA0_SRCSA);               

       /*
         Set 'Half Terminal Count Interrupt Enable' and 'Terminal
         Count Interrupt Enable' in DMA Control Registers 0 & 1.
         Set 'MSK' flag to 1 to keep the DMA engines paused.
       */
      /* temp1 = (DCRn_TCIE | DCRn_MSK);    
       writel(temp1, CS4281_pBA0 + BA0_DCR0);*/          
       temp1 = (DCRn_HTCIE | DCRn_TCIE | DCRn_MSK);    
       writel(temp1, CS4281_pBA0 + BA0_DCR0);          

     /*  temp1 = (DCRn_HTCIE | DCRn_TCIE | DCRn_MSK);    
       writel(temp1, CS4281_pBA0 + BA0_DCR1);  */        
       temp1 = (DCRn_TCIE | DCRn_MSK);    
       writel(temp1, CS4281_pBA0 + BA0_DCR1);          

       /*
         Set 'Auto-Initialize Control' to 'enabled'; For playback,
         set 'Transfer Type Control'(TR[1:0]) to 'read transfer',
         for record, set Transfer Type Control to 'write transfer'.
         All other bits set to zero;  Some will be changed @ transfer start.
       */
       temp1 = (DMRn_DMA | DMRn_AUTO | DMRn_TR_READ);   
       writel(temp1, CS4281_pBA0 + BA0_DMR0);            

       temp1 = (DMRn_DMA | DMRn_AUTO | DMRn_TR_WRITE);   
       writel(temp1, CS4281_pBA0 + BA0_DMR1);            

       /*
         Enable DMA interrupts generally, and
         DMA0 & DMA1 interrupts specifically.
       */
       temp1 = readl(CS4281_pBA0 + BA0_HIMR) &  0xfffbfcff;
       writel(temp1, CS4281_pBA0+BA0_HIMR);
       

    printf("\nCS4281 Hardware Initialization Complete!!\n");
    return 0;
}	


/*-------------------------
  Start DAC / Stop DAC
  ------------------------*/

void start_dac(void)
{
    UINT32 temp1;
    int lockkey;
    
    lockkey = intLock();
    
    /* enable DMA0 */
    temp1 = readl(CS4281_pBA0+BA0_DCR0);
    writel(temp1 & ~DCRn_MSK, CS4281_pBA0+BA0_DCR0);

    /* enable DMA1 */
    temp1 = readl(CS4281_pBA0+BA0_DCR1);
    writel(temp1 & ~DCRn_MSK, CS4281_pBA0+BA0_DCR1);

    /* enable Interrupt */
    writel(HICR_IEV | HICR_CHGM, CS4281_pBA0+BA0_HICR);
    writel(0, CS4281_pBA0+BA0_PPRVC);
    writel(0, CS4281_pBA0+BA0_PPLVC);
    
    intUnlock(lockkey);
}

/*-------------------------
  Start ADC / Stop ADC
  ------------------------*/

void start_adc(void)
{
  /*  UINT32 temp1;*/
    int lockkey;
    
    lockkey = intLock();
    

    /* enable Interrupt */
    writel(HICR_IEV | HICR_CHGM, CS4281_pBA0+BA0_HICR);
    writel(0, CS4281_pBA0+BA0_PPRVC);
    writel(0, CS4281_pBA0+BA0_PPLVC);
    intUnlock(lockkey);
}


/*-------------------------
 set  DMAcntl, SampleRate,
 <8bits unsigned mono>
  ------------------------*/
int prog_codec(void)
{
    UINT32 format;
    int lockkey;
 
    lockkey = intLock();

    /* Program record/playback sampling rate -- 11025 */


    /* DAC */
    format = DMRn_DMA | DMRn_AUTO |DMRn_TR_READ|DMRn_USIGN|DMRn_SIZE8|DMRn_MONO;
    writel(format, CS4281_pBA0+BA0_DMR0);
    
    writel(0x04, CS4281_pBA0+BA0_DACSR);

    /* ADC */
    format = DMRn_DMA | DMRn_AUTO |DMRn_TR_WRITE/*|DMRn_USIGN*/|DMRn_SIZE8|DMRn_MONO;
    writel(format, CS4281_pBA0+BA0_DMR1);

    writel(0x04, CS4281_pBA0+BA0_ADCSR);

    intUnlock(lockkey);
    return 0;
}




/*------------------------------------*
 * Interrupt Service Routine
 *    for
 *        DMA0, DMA1
 *------------------------------------*/
void cs4281_interrupt(int param)
{
    UINT32 temp1;
  /*  int lockkey;*/

	
    /*logMsg("\n CW:cs4281--> \n");*/

    /* if it's not a DMA interrupt, jump out by issuing a EOI */
    temp1 = readl(CS4281_pBA0+BA0_HISR);
    if(!(temp1 &(HISR_DMA0 | HISR_DMA1))) {
	writel(HICR_IEV| HICR_CHGM, CS4281_pBA0+BA0_HICR);
	return;
    }

    
    /* playback DMA0 */
    if(temp1 & HISR_DMA0) {
        semGive(SEM_DMA_Playback);
	CNT_DMA_Playback++;
	
	if(0x00010000 &	readl(CS4281_pBA0+BA0_HDSR0)) {
	    /*DMA Terminal Count */
	    DTC_DMA_Playback = 1;
	   /* logMsg("playbkDMA0:TC");*/
	}
	else {
	    DTC_DMA_Playback = 0;
/*	    logMsg("playbkDMA0:Half_TC");*/
	    }
    }
    /* record DMA1 */
    if(temp1 & HISR_DMA1) {
        semGive(SEM_DMA_Record);
	CNT_DMA_Record++;
	
	if(0x00010000 &	readl(CS4281_pBA0+BA0_HDSR1)) {
	    /*DMA Terminal Count */
	    DTC_DMA_Record = 1;
	    /*logMsg("recordDMA1:TC");*/
	}
	else {
	    DTC_DMA_Record = 0;
	    /*logMsg("recordDMA1:Half_TC");*/
	    }
    }

/*    logMsg("\n");*/
   writel(HICR_IEV| HICR_CHGM, CS4281_pBA0+BA0_HICR);
}
	
    
/*********************************************************
 * Install the cs4281/ac97 driver:
 *   -bring up the hardware
 *   -allocate and set DMA buffer/DMA engine
 *   -connect ISR to IRQ
 *   -start_adc/dac
 ********************************************************/
 
    
int probe1()
{
    unsigned int pciBusNo, pciDevNo, pciFuncNo;
    unsigned char byte;
   /* int           i,j;*/
    UINT32 s_pBA0, s_pBA1;

    /* To ensure that taskDelay(1) is 1ms delay */
    sysClkRateSet(1000);
    if (pciConfigLibInit (PCI_MECHANISM_1, 0xCF8, 0xCFC, 0) != OK) {
	printf("PCI lib config error\n");
	return 1;
    }

    /****************************
     * Find SoundCard
     * Set  BaseAddr0, BaseAddr1
     ****************************/
    if(!(pciFindDevice(PCI_VENDOR_ID_CIRRUS,PCI_DEVICE_ID_CRYSTAL_CS4281,
		       0, &pciBusNo, &pciDevNo, &pciFuncNo)==OK)) {
	printf("\n CS4281 sound card NOT FOUND!!! \n");
	return 1;
    }

    printf("\n FOUND CS4281 sound card, configuring BA0,BA1... \n");    
    pciConfigOutLong( pciBusNo, pciDevNo, pciFuncNo,
		      PCI_CFG_BASE_ADDRESS_0, CS4281_pBA0);
    /*
    pciConfigOutLong( pciBusNo, pciDevNo, pciFuncNo,
		      PCI_CFG_BASE_ADDRESS_1, CS4281_pBA1);
    */
    pciConfigInLong( pciBusNo, pciDevNo, pciFuncNo,
		     PCI_CFG_BASE_ADDRESS_0, &s_pBA0);
    pciConfigInLong( pciBusNo, pciDevNo, pciFuncNo,
		     PCI_CFG_BASE_ADDRESS_1, &s_pBA1 );
    printf ("\npBusNo    pDeviceNo  pFuncNo  pBA0      pBA1\n\n");
    printf ("%.8x  %.8x  %.8x  %.8x  %.8x \n",
	    pciBusNo, pciDevNo, pciFuncNo, s_pBA0,s_pBA1); 


    

    /********************************
     * Config PCI Device Capability
     *     DMA Master
     *     MEM mapped
     ********************************/

    /* Set the INTA vector */
    pciConfigInByte(pciBusNo, pciDevNo, pciFuncNo,
		    PCI_CFG_DEV_INT_LINE, &cs4281_irq);
    printf("\nFound CS4281 configured for IRQ %d\n", cs4281_irq);
    
    pciConfigInByte(pciBusNo, pciDevNo, pciFuncNo,
		    PCI_CFG_DEV_INT_PIN, &byte);
    printf("\tINT_PIN=%.8x\n", byte);

    /* Enable the device's capabilities as specified
     * Bus Master Enable/ Mem Space Enable */
    pciConfigOutWord(pciBusNo, pciDevNo, pciFuncNo, PCI_CFG_COMMAND,
		     (unsigned short)0x0006);

    
    /***************************
     * BringUp Hardware 
     ***************************/
    /*Include Init Function Here*/
    cs4281_hw_init();

    /****************************
     * Allocate ADC_BUFFER
     * Allocate DAC_BUFFER
     *
     * Hook cs4281_interrupt
     *
     * Program CoDec
     ****************************/
    if((DAC_BUFFER=valloc(DAC_BUFFER_SIZE))==NULL) {
       printf("\n DAC_BUFFER valloc failed!\n");
       return 1;
       
    }
    memset( DAC_BUFFER, 0, DAC_BUFFER_SIZE );
    printf("\ndac=%x",DAC_BUFFER);
    /*for( i=0 ; i < DAC_BUFFER_SIZE  ; i++ )
        ((char *)DAC_BUFFER)[i]=0x7f;*/

    writel((UINT32)DAC_BUFFER+8,CS4281_pBA0 + BA0_DBA0);
    writel(DAC_BUFFER_SIZE-16, CS4281_pBA0 + BA0_DBC0);
    printf("\nbao=%x",readl(CS4281_pBA0 + BA0_DBA0));
    printf("\nbco=%x",readl(CS4281_pBA0 + BA0_DBC0));
    printf("\ncco=%x",readl(CS4281_pBA0 + BA0_DCC0));
    if((ADC_BUFFER=valloc(ADC_BUFFER_SIZE))==NULL) {
       printf("\n ADC_BUFFER valloc failed!\n");
       return 1;
    }
    printf("\nadc=%x",ADC_BUFFER);
    /*for( i=0 ; i < ADC_BUFFER_SIZE  ; i++ )
        ((char *)ADC_BUFFER)[i]=0x7f;*/

    writel((UINT32)ADC_BUFFER+8,CS4281_pBA0 + BA0_DBA1);
    writel(ADC_BUFFER_SIZE-16, CS4281_pBA0 + BA0_DBC1);
    
    /* connect interrupt */
    printf("\n Hook cs4281_interrupt to vector %d\n",
	   (INUM_TO_IVEC (cs4281_irq+INT_NUM_IRQ0)));
    pciIntConnect((INUM_TO_IVEC (cs4281_irq+INT_NUM_IRQ0)),
		(VOIDFUNCPTR)cs4281_interrupt, 0);
    sysIntEnablePIC(cs4281_irq);
    
    SEM_DMA_Playback = semBCreate(SEM_Q_FIFO,SEM_EMPTY);
    SEM_MY_Playback = semBCreate(SEM_Q_FIFO,SEM_EMPTY);
    SEM_DMA_Record = semBCreate(SEM_Q_FIFO,SEM_EMPTY);
    SEM_Sample = semBCreate(SEM_Q_FIFO,SEM_EMPTY);

    CNT_DMA_Playback = CNT_DMA_Record = 0;
    
    /* program coDec */
    printf("\n Program CoDec (sample rate, DMA...)\n");
    prog_codec();


    /*********************************************
     *  start dac/adc, interrupt is comming...
     *********************************************/
    start_dac();
  	return 0;
}


void loadbuffer(char *b , int size)
{
  int i;

  for(i=0;i<size;i++)
  {
     b[i] = bugs[(j+i)% BUG_SIZE];
  }j = j +size;

} 

void start(void)
 {
	   int x;
	   x=probe1();
	   printf("\n x:%d",x);
 }



int RecvTask(   int sockfd/* server's socket fd */  )
{
	/*int i,j;*/
        while(1) 
        {
            /*nRead = read(newFd, adNauseam, sizeof (adNauseam));
            if ( nRead < 0 )
            {
                perror ("read");
                close (sFd);
                return (ERROR);
	    }*/
        static int kk=0;
        kk++;
            semTake(SEM_DMA_Playback, WAIT_FOREVER);
           /* semTake(SEM_Sample, WAIT_FOREVER);*/
            loadbuffer(saved_samples, DAC_BUFFER_SIZE);
	    	if(DTC_DMA_Playback)
	    	{
	    		printf("\e[1;1H task0=%d     %d \n", kk);
                memmove(DAC_BUFFER+(DAC_BUFFER_SIZE/2),saved_samples/*adNauseam*/, (DAC_BUFFER_SIZE/2));
	    	}
            else
            {
            	printf("\e[1;40H task1=%d    [%d]  \n", kk );
                memmove(DAC_BUFFER,/*adNauseam*/saved_samples, (DAC_BUFFER_SIZE/2));
            }
	   

        } /* end while awaiting frames */

        return 0;
}

int SendTask(   int sockfd/* server's socket fd */  )
{
	/*char * tmp_buf;
	int i,flag1=1;*/
    
    /*loadBuffer(saved_samples);*/
    while(1)
    {	
    
        semTake(SEM_DMA_Record, WAIT_FOREVER);
        /* loadbuffer(saved_samples); */
        if(DTC_DMA_Record)
	{
	    /*tmp_buf=(char *) ADC_BUFFER+(ADC_BUFFER_SIZE/2);*/
            memmove(saved_samples, ADC_BUFFER+(ADC_BUFFER_SIZE/2), (ADC_BUFFER_SIZE/2));

	}
        else
	{
	    /*tmp_buf=(char *)ADC_BUFFER;*/
            memmove(saved_samples, ADC_BUFFER, (ADC_BUFFER_SIZE/2) );
            
            
	}    	
    semGive(SEM_Sample);
        
       
    }
    return 0;
}

void start1()
{
if (taskSpawn("RecvTask", 5, 0, SERVER_STACK_SIZE,(FUNCPTR) RecvTask, 0, 0, 0,0 ,0, 0, 0, 0, 0, 0) == ERROR)
    {
      perror ("taskSpawn");
    }
}

void ee(void)
{
	int taskId;
	UINT32 temp1;
	int lockkey;
	/* Disable Interrupts*/
	
	/* stop DMA0 */
	    temp1 = readl( CS4281_pBA0 + BA0_DCR0 );
	    writel( temp1 | 1, CS4281_pBA0 + BA0_DCR0 );
	 
	    /* stop DMA1 */
	    temp1 = readl( CS4281_pBA0 + BA0_DCR1 );
	    writel( temp1 | 1, CS4281_pBA0 + BA0_DCR1 );
	
	writel( 0, CS4281_pBA0 + BA0_HICR );
	/* Kill Tasks*/
	if ( (taskId = taskNameToId( "RecvTask" )) != ERROR )
	    {
	        if ( taskDelete( taskId ) != ERROR )
	            printf( "Old RecvTask deleted.\n" );
	    }
	intUnlock(lockkey);
}

/*char foo()
{
    return -1;
}

void char_test()
{
    if(foo() == -1)
        printf("Bang\n");
    else
        printf("Splat\n");
}*/


/*
 * zero ADC_BUFFER
 * GAINS and ATTenuations important- recommends setting mid-level
 * Linux Driver documented www.alsa.com -RECORD_GAIN_MIC ( getting rid of analog loopback)
 * Sampling rate = interrupt rate; crunch the numbers...
 * Double check mono setting
 * wvEvent calls to know where you are in windView
 * set task priorities to be RM type priorities
 * tNetTask priority - keep it in mind when setting priorites of send/recv.
 * buffer sizes are important - find the sweet spot
 */
