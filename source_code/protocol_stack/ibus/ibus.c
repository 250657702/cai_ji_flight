#include "ibus.h"



E_IBUS_ERROR_TYPE IBUS_AnalysisData(IBUS_t * ibus, uint8_t * data, uint32_t len, uint8_t * const isGetPackage)
{
	//int ret = 1;
	uint32_t i;
	uint16_t checkSum;

	*isGetPackage = FALSE;

	if(NULL == ibus)
	{
		return E_IBUS_ERROR_NULL;
	}

	for(i = 0;i < len;i++)
	{
		//头
		if((0 == ibus->NowDataIndex) && (0x20 == *data))
		{
			ibus->DataBuffer[0] = *data;
			++ibus->NowDataIndex;
		}
		//头
		else if((1 == ibus->NowDataIndex) && (0x40 == *data))
		{
			ibus->DataBuffer[1] = *data;
			ibus->CheckSum = 0;
			++ibus->NowDataIndex;
		}
		//数据
		else if((ibus->NowDataIndex > 1) && (ibus->NowDataIndex < 30))    //30
		{
			ibus->DataBuffer[ibus->NowDataIndex] = *data;
		  	ibus->CheckSum += *data;
		  	++ibus->NowDataIndex;
		}
		else if(30 == ibus->NowDataIndex)
		{
			ibus->DataBuffer[ibus->NowDataIndex] = *data;
			++ibus->NowDataIndex;
		}
		//数据校验
		else if(31 == ibus->NowDataIndex)
		{
			ibus->DataBuffer[ibus->NowDataIndex] = *data;
			ibus->NowDataIndex = 0;
			
			checkSum = (((uint16_t)ibus->DataBuffer[31]) << 8) | ibus->DataBuffer[30];
  			ibus->CheckSum = (ibus->CheckSum + 0x60) ^ 0xffff;
			if(ibus->CheckSum == checkSum)
			{
				for(i = 0;i < E_IBUS_Channel_MAX;i++)
			    {
					ibus->ChannelData[i] = ((uint16_t)(ibus->DataBuffer[(i << 1) + 3]) & 0x0f) << 8 | (ibus->DataBuffer[(i << 1) + 2]);
			    }

				*isGetPackage = TRUE;

				if(NULL != ibus->ReceivedPackageFunc)
				{
					ibus->ReceivedPackageFunc(ibus->ChannelData);
				}
			}
		}
		else
		{
			ibus->NowDataIndex = 0;
		}
		
		++data;
	}

  	//return ret;
}




uint16_t IBUS_GetChannelData(IBUS_t * ibus, E_IBUS_Channel channel)
{
	if(NULL == ibus)
	{
		return 0;
	}
	else
	{
		return ibus->ChannelData[channel];
	}
	
}


