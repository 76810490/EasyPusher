/*
	Copyright (c) 2013-2014 EasyDarwin.ORG.  All rights reserved.
	Github: https://github.com/EasyDarwin
	WEChat: EasyDarwin
	Website: http://www.EasyDarwin.org
*/
#include "StdAfx.h"
#include "SourceManager.h"
#include "EasyPusherDlg.h"
#include "YUVTransform.h"
#include "vdev.h"

#define KEY "6A36334A743469576B5A73415162316270454B396B76464659584E355548567A614756794C6D56345A534E58444661672F704C6A5257467A65555268636E6470626C526C5957314A6331526F5A554A6C633352414D6A41784F47566863336B3D"

CSourceManager* CSourceManager::s_pSourceManager = NULL;

CSourceManager::CSourceManager(void)
{
	m_bPushing = FALSE;
	m_sPushInfo.pusherHandle = 0;
	//��Ƶ�豸����ʵ��
	m_pVideoManager = NULL;
	//��Ƶ�豸����ʵ��
	m_pAudioManager = NULL;
	m_bDSCapture = FALSE;
	m_hFfeVideoHandle = NULL;
	m_hFfeAudioHandle = NULL;
	m_nFrameNum = 0;
	m_EncoderBuffer = NULL;
	m_pScreenCaptrue = NULL;
	m_pD3dScreenCaptrue = NULL;
	m_nScreenCaptureId = -1;
	m_pMP4Writer = NULL;
	m_bRecording = FALSE; 
	m_handler = NULL;
	m_bUseGpac = TRUE;
	m_bWriteMp4 = FALSE;
	m_bPushRtmp = FALSE;
	m_nEncoderType = 0; //Ĭ��H264����
	 
	m_pEncConfigInfo = NULL;
	m_pFrameBuf = NULL;
	m_pEasyrtmp = NULL;
	m_bUseFFEncoder = FALSE;
	m_hCaptureWnd = NULL;
	m_pEasyFileCapture = NULL;
	m_pVFXMaker = NULL;
	memset(&m_vfxConfigInfo, 0x00, sizeof(VFXMakerInfo));
}

CSourceManager::~CSourceManager(void)
{
}

//��ʼ��Ψһ�Ĺ���ʵ��
CSourceManager* CSourceManager::Instance()
{
	if (!s_pSourceManager)
	{
		s_pSourceManager = new CSourceManager();
		s_pSourceManager->InitSource();
	}
	return s_pSourceManager;
}

//����Ψһ�Ĺ���ʵ��
 void CSourceManager::UnInstance()
{
	if (s_pSourceManager)
	{
		s_pSourceManager->RealseManager();

		delete s_pSourceManager;
		s_pSourceManager = NULL;
	}
}

//�ͷ�Master��ռ�����Դ
void CSourceManager::RealseManager()
{
	StopPlay();
	StopPush();
	StopCapture();
	RealseScreenCapture();
	UnInitSource();
}

//ִ����ƵԴ��ʼ������
void CSourceManager::InitSource()
{
	RGB_InitDraw(&m_d3dHandle);
	int nRet = EasyPlayerManager::Init();
	int isActivated = EasyPusher_Activate(KEY);
	switch(isActivated)
	{
	case EASY_ACTIVATE_INVALID_KEY:
		printf("KEY is EASY_ACTIVATE_INVALID_KEY!\n");
		break;
	case EASY_ACTIVATE_TIME_ERR:
		printf("KEY is EASY_ACTIVATE_TIME_ERR!\n");
		break;
	case EASY_ACTIVATE_PROCESS_NAME_LEN_ERR:
		printf("KEY is EASY_ACTIVATE_PROCESS_NAME_LEN_ERR!\n");
		break;
	case EASY_ACTIVATE_PROCESS_NAME_ERR:
		printf("KEY is EASY_ACTIVATE_PROCESS_NAME_ERR!\n");
		break;
	case EASY_ACTIVATE_VALIDITY_PERIOD_ERR:
		printf("KEY is EASY_ACTIVATE_VALIDITY_PERIOD_ERR!\n");
		break;
	case EASY_ACTIVATE_SUCCESS:
		printf("KEY is EASY_ACTIVATE_SUCCESS!\n");
		break;
	}

	if(EASY_ACTIVATE_SUCCESS != isActivated)
		return ;
}
//����ʼ��
void CSourceManager::UnInitSource()
{
	//m_AACEncoderManager.Clean();
	RGB_DeinitDraw(&m_d3dHandle);
	EasyPlayerManager::UnInit();
}

unsigned int CALLBACK CSourceManager::CaptureFileCallBack(int nDevId, EASY_AV_Frame* frameInfo, void* pMaster)
{
	if (!pMaster || !frameInfo->pBuffer || frameInfo->u32AVFrameLen <=0)
	{
		return 0;
	}

	CSourceManager* pManager = (CSourceManager*)pMaster;
	if (pManager)
	{
		pManager->	CaptureFileManager(nDevId, frameInfo, pMaster);
	}

	return 1;
}

void CSourceManager::CaptureFileManager(int nDevId, EASY_AV_Frame* frameInfo, void* pMaster)
{
	RTSP_FRAME_INFO _frameInfo;
	memset(&_frameInfo, 0x00, sizeof(RTSP_FRAME_INFO));
	_frameInfo.length = frameInfo->u32AVFrameLen ;
	_frameInfo.timestamp_sec = frameInfo->u32TimestampSec ;
	_frameInfo.timestamp_usec = frameInfo->u32TimestampUsec ;
	_frameInfo.type = frameInfo->u32VFrameType ;
	_frameInfo.codec = frameInfo->u32AVFrameFlag;
	SourceManager(nDevId, (int*)&m_sSourceInfo , frameInfo->u32AVFrameFlag, (char*)frameInfo->pBuffer, &_frameInfo);
}


int CALLBACK CSourceManager::CaptureScreenCallBack(int nId, unsigned char *pBuffer, int nBufSize,  RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo, void* pMaster)
{
	if (!pBuffer || nBufSize <= 0)
	{
		return -1;
	}

	//ת����ǰʵ�����д���
	CSourceManager* pThis = (CSourceManager*)pMaster;
	if (pThis)
	{
		pThis->CaptureScreenManager(nId, pBuffer, nBufSize,  realDataType, realDataInfo);
	}
	return 1;
}
void CSourceManager::CaptureScreenManager(int nId, unsigned char *pBuffer, int nBufSize,  RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo)
{
	ScreenCapDataInfo* pDataInfo = (ScreenCapDataInfo*)realDataInfo;
	if (pDataInfo&&m_hCaptureWnd)
	{
		RECT rcClient;
		SetRect(&rcClient, 0, 0, pDataInfo->nWidth, pDataInfo->nHeight);
		RGB_DrawData(m_d3dHandle, m_hCaptureWnd, (char*)pBuffer, pDataInfo->nWidth, pDataInfo->nHeight, &rcClient, 0x01, RGB(0x00,0x00,0x00), 0x01);
	}

	DSRealDataManager(nId, pBuffer,nBufSize,  realDataType, realDataInfo );
}

int CALLBACK CSourceManager::__MediaSourceCallBack( int _channelId, int *_channelPtr, int _frameType, char *pBuf, RTSP_FRAME_INFO* _frameInfo)
{
	//ת����ǰʵ�����д���
	EASY_LOCAL_SOURCE_T *pLocalSource = (EASY_LOCAL_SOURCE_T *)_channelPtr;
	if (pLocalSource)
	{
		CSourceManager* pMaster = (CSourceManager*)pLocalSource->pMaster;
		if (pMaster)
		{
			pMaster->SourceManager(_channelId, _channelPtr, _frameType, pBuf, _frameInfo);
		}
	}
	
	return 0;
}

//ʵʱ���ݻص�����
int  CSourceManager::RealDataCallbackFunc(int nDevId, unsigned char *pBuffer, int nBufSize, 
																		RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo, void* pMaster)
{

	if (!pBuffer || nBufSize <= 0)
	{
		return -1;
	}
	//ת����ǰʵ�����д���
	CSourceManager* pThis = (CSourceManager*)pMaster;
	if (pThis)
	{
		pThis->DSRealDataManager(nDevId, pBuffer, nBufSize, realDataType, realDataInfo);
	}

	return 0;
}

void CSourceManager::DSRealDataManager(int nDevId, unsigned char *pBuffer, int nBufSize, 
										RealDataStreamType realDataType, /*RealDataStreamInfo*/void* realDataInfo)
{
	int nVideoWidth = 640;
	int nVideoHeight = 480;
	int nFps = 25;

	nVideoWidth = m_sDevConfigInfo.VideoInfo.nWidth ;
	nVideoHeight = m_sDevConfigInfo.VideoInfo.nHeight ;
	nFps = m_sDevConfigInfo.VideoInfo.nFrameRate;

	switch (realDataType)
	{
	case REALDATA_VIDEO:
		{
			if (m_bPushing)
			{
				//YUV��ʽת��
				int nWidhtHeightBuf=(nVideoWidth*nVideoHeight*3)>>1;
				CString strDataType = _T("");
				strDataType = m_sDevConfigInfo.VideoInfo.strDataType;
				BYTE* pDataBuffer = NULL;
				BYTE* pDesBuffer = pBuffer;
				BYTE* pVfxBuffer = NULL;
				int nBufSize = nVideoWidth*nVideoHeight << 1;
				if (!pVfxBuffer)
				{
					pVfxBuffer = (BYTE*)malloc(nBufSize); //����д��Դ���� 
					memset(pVfxBuffer, 0x00, nBufSize);
				}
				if (strDataType == _T("YUY2"))
				{
					memcpy(pVfxBuffer, pBuffer, nBufSize);
// 					pDataBuffer=new unsigned char[nWidhtHeightBuf];
// 					YUY2toI420(nVideoWidth,nVideoHeight, pVfxBuffer, pDataBuffer);
// 					pDesBuffer = pDataBuffer;
				}
				else //Ĭ��==RGB24
				{
					//RGB24->YUY2
					vdev_convert( AV_PIX_FMT_BGR24 , nVideoWidth,nVideoHeight, pBuffer, AV_PIX_FMT_YUYV422, nVideoWidth,nVideoHeight, &pVfxBuffer);

// 					pDesBuffer = pBuffer;
// 					if (!m_bUseFFEncoder)//x264 Encoder just suppport i420 CSP 
// 					{
// 						pDataBuffer=new unsigned char[nWidhtHeightBuf];
// 						// rgb24->i420
// 						ConvertRGB2YUV(nVideoWidth,nVideoHeight, pBuffer, (unsigned char*)pDataBuffer);
// 						pDesBuffer = pDataBuffer;
// 					}
				}
				//YUY2�������ؼ�����
#if 1
				WaterMarkInfo waterMarkInfo = m_vfxConfigInfo.warkMarkInfo;
				if (waterMarkInfo.bIsUseWaterMark)
				{
					if (waterMarkInfo.bResetWaterMark )
					{
						//��ʼ��ˮӡ����
						//;��ʾ̨��λ�ã�1 == ���� 2 == ���� 3 == ���� 4 == ����
						//eWaterMarkPos = 3

						//;ˮӡ����x�����꣬���鲻С��0����������Ƶ���
						//nLeftTopX = 0

						//;ˮӡ����y�����꣬���鲻С��0����������Ƶ�߶�
						//nLeftTopY = 480

						//;ˮӡ���0 - 6
						//eWatermarkStyle = 3

						//;ˮӡͼ���ļ�·��LOGO.png
						//strWMFilePath = .\Res\logo.png
						switch (waterMarkInfo.eWaterMarkPos)
						{
						case POS_LEFT_TOP:
							waterMarkInfo.nLeftTopX = 0;
							waterMarkInfo.nLeftTopY = 0;
							break;
						case POS_RIGHT_TOP:
							waterMarkInfo.nLeftTopX = nVideoWidth;
							waterMarkInfo.nLeftTopY = 0;
							break;
						case POS_LEFT_BOTTOM:
							waterMarkInfo.nLeftTopX = 0;
							waterMarkInfo.nLeftTopY = nVideoHeight;
							break;
						case POS_RIGHT_BOTTOM:
							waterMarkInfo.nLeftTopX = nVideoWidth;
							waterMarkInfo.nLeftTopY = nVideoHeight;
							break;
						}

						m_pVFXMaker->SetVideoInVideoParam( 101, 0, 0, nVideoWidth, nVideoHeight, 100, 100, 100);

						m_pVFXMaker->SetLogoImage(waterMarkInfo.strWMFilePath, waterMarkInfo.nLeftTopX,
							waterMarkInfo.nLeftTopY, waterMarkInfo.bIsUseWaterMark, waterMarkInfo.eWatermarkStyle);

						m_vfxConfigInfo.warkMarkInfo.bResetWaterMark = FALSE;
					}
				}

				//��ʼ����Ļ��Ϣ
				VideoTittleInfo tittleInfo = m_vfxConfigInfo.tittleInfo;
				if(tittleInfo.bResetTittleInfo)
				{

					// 	-->1����ʼ��������Ļָ��,����ʼ����Ƶ�������		m_pVideoVfxMakerInfo->nDesWidth, m_pVideoVfxMakerInfo->nDesHeight,  m_pVideoVfxMakerInfo->strDesBytesType);
					m_pVFXMaker->CreateOverlayTitle(nVideoWidth, nVideoHeight, ("YUY2"));

					// 	-->2��������Ļ������Ϣ
					LOGFONTA inFont;
					inFont.lfHeight      = tittleInfo.nTittleHeight;
					inFont.lfWidth       = tittleInfo.nTittleWidth;
					inFont.lfEscapement  = 0;
					inFont.lfOrientation = 0;
					inFont.lfWeight      = tittleInfo.nFontWeight;//FW_NORMAL;
					inFont.lfItalic      = 0;
					inFont.lfUnderline   = 0;
					inFont.lfStrikeOut   = 0;
					inFont.lfCharSet        =GB2312_CHARSET;// ANSI_CHARSET;//134
					inFont.lfOutPrecision   =3;// OUT_DEFAULT_PRECIS;
					inFont.lfClipPrecision  = 2;//CLIP_DEFAULT_PRECIS;
					inFont.lfQuality        = 1;//PROOF_QUALITY;
					inFont.lfPitchAndFamily = 0;//49;//49

					strcpy(inFont.lfFaceName, tittleInfo.strFontType);//"������κ");//"��������");"����"

					POINT pointTitle;

					if(tittleInfo.nMoveType==0)
					{
						pointTitle=	tittleInfo.ptStartPosition;
						if(pointTitle.x<=0) pointTitle.x=1;
						if(pointTitle.x>=nVideoWidth) pointTitle.x=nVideoWidth/2;
					}
					else if(tittleInfo.nMoveType==1)//��������
					{
						pointTitle.x =	-1;
						pointTitle.y = tittleInfo.ptStartPosition.y;
					}
					else if(tittleInfo.nMoveType==2)
					{
						pointTitle.x =	nVideoWidth+1;
						pointTitle.y = tittleInfo.ptStartPosition.y;
					}

					m_pVFXMaker->SetOverlayTitleInfo(tittleInfo.strTittleContent, 
						inFont, tittleInfo.nColorR, tittleInfo.nColorG,
						tittleInfo.nColorB, pointTitle);

					//-->3��������Ļ����ץ״̬
					m_pVFXMaker->SetOverlayTitleState(tittleInfo.nState);

					m_vfxConfigInfo.tittleInfo.bResetTittleInfo = FALSE;
				}

				if (m_pVFXMaker && (waterMarkInfo.bIsUseWaterMark || tittleInfo.nState))//logo-ˮӡ + ��Ļ + ������
				{

					//int nBufSize = nVideoWidth*nVideoHeight << 1;
					// 						if (!pVfxBuffer)
					// 						{
					// 							pVfxBuffer = (BYTE*)malloc(nBufSize); //����д��Դ���� 
					// 							memset(pVfxBuffer, 0x00, nBufSize);
					// 						}
					// 						memcpy(pVfxBuffer, pBuffer, nBufSize);

					//ˮӡ����
					if(waterMarkInfo.bIsUseWaterMark)
						m_pVFXMaker->AddWaterMask(pVfxBuffer);
					//OSD����
					if(tittleInfo.nState)
						m_pVFXMaker->DoOverlayTitle(pVfxBuffer);
				}
#endif
				pDataBuffer=new unsigned char[nWidhtHeightBuf];
				vdev_convert( AV_PIX_FMT_YUYV422, nVideoWidth,nVideoHeight, pVfxBuffer, AV_PIX_FMT_YUV420P, nVideoWidth,nVideoHeight, &pDataBuffer);
				pDesBuffer = pDataBuffer;
				// 					
				RTSP_FRAME_INFO	frameinfo;
				memset(&frameinfo, 0x00, sizeof(RTSP_FRAME_INFO));
				int datasize=0;
				bool keyframe=false;
				if (m_bUseFFEncoder)
				{			
					if (m_hFfeVideoHandle)
					{
						//FFEncoder����
						int ret = FFE_EncodeVideo(m_hFfeVideoHandle, pDesBuffer, (unsigned char*)m_EncoderBuffer, &datasize, ++m_nFrameNum, 1);
						if (ret == 0x00 && datasize>0)
						{
							keyframe = ((unsigned char)m_EncoderBuffer[4]==0x67?true:false);
							frameinfo.length = datasize;
						}
					}
				}
				else//x264����
				{
					byte*	 pdata = NULL;
					if (m_nEncoderType == 1)
					{
						pdata = m_H265Encoder.Encoder(pDataBuffer, nWidhtHeightBuf, datasize, keyframe);
					}
					else
					{
						pdata = m_H264EncoderManager.Encoder(0, pDataBuffer,
							nWidhtHeightBuf, datasize, keyframe);
					}
					if (datasize>0)
					{
						memset(m_EncoderBuffer, 0, 2073600);//1920*1080
						bool bKeyF = keyframe;
						byte btHeader[4];
						btHeader[0] = 0x00;
						btHeader[1] = 0x00;
						btHeader[2] = 0x00;
						btHeader[3] = 0x01;
						if (bKeyF)
						{

							int nOffSet = 0;
							if (m_nEncoderType == 0)//h264
							{
// 								memcpy(m_EncoderBuffer+nOffSet, btHeader, 4);
// 								nOffSet += 4;
// 								memcpy(m_EncoderBuffer + nOffSet, m_vps, m_vpslen);
// 								nOffSet += m_vpslen;
								memcpy(m_EncoderBuffer + nOffSet, btHeader, 4);
								nOffSet += 4;
								memcpy(m_EncoderBuffer + nOffSet, m_sps, m_spslen);
								nOffSet += m_spslen;
								memcpy(m_EncoderBuffer + nOffSet, btHeader, 4);
								nOffSet += 4;
								memcpy(m_EncoderBuffer + nOffSet, m_pps, m_ppslen);
								nOffSet += m_ppslen;
							}

							memcpy(m_EncoderBuffer + nOffSet, btHeader, 4);
							nOffSet += 4;					
							memcpy(m_EncoderBuffer + nOffSet, pdata+4, datasize-4);
							nOffSet += datasize - 4;
							frameinfo.length = nOffSet;

						} 
						else
						{
							frameinfo.length = datasize;
							memcpy(m_EncoderBuffer, btHeader, 4);
							memcpy(m_EncoderBuffer+4, pdata+4, datasize-4);
						}
					}
				}
				if (datasize>0&&m_EncoderBuffer)
				{
					frameinfo.codec = EASY_SDK_VIDEO_CODEC_H264;
					frameinfo.width  = nVideoWidth;
					frameinfo.height = nVideoHeight;
					frameinfo.fps    = nFps;
					frameinfo.type	 = ( keyframe ? EASY_SDK_VIDEO_FRAME_I : EASY_SDK_VIDEO_FRAME_P);
					long nTimeStamp = clock();
					frameinfo.timestamp_sec = nTimeStamp/1000;
					frameinfo.timestamp_usec = (nTimeStamp%1000)*1000;

					frameinfo.sample_rate = m_sDevConfigInfo.AudioInfo.nSampleRate;
					frameinfo.channels = m_sDevConfigInfo.AudioInfo.nChannaels;
					//���ͻص�����
					//TRACE("frameinfo.length == %d \r\n", frameinfo.length);
					SourceManager(nDevId, (int*)&m_sSourceInfo, EASY_SDK_VIDEO_FRAME_FLAG, m_EncoderBuffer, &frameinfo);
				}

				if (pDataBuffer)
				{
					delete[] pDataBuffer;
					pDataBuffer = NULL;
				}	
				if(pVfxBuffer)
				{
					free(pVfxBuffer);
					pVfxBuffer = NULL;

				}
			}
		}
		break;
	case REALDATA_AUDIO:
		{
			if (m_bPushing)
			{
				RTSP_FRAME_INFO	frameinfo;
				memset(&frameinfo, 0x00, sizeof(RTSP_FRAME_INFO));
				int datasize=0;
				unsigned char *pAACbuf= NULL;
				if (m_bUseFFEncoder)//FFEncoder 
				{
					if (m_hFfeAudioHandle)
					{
						//��Ƶ����
						int ret = AAC_Encode(m_hFfeAudioHandle, (int*)pBuffer, nBufSize, &pAACbuf, &datasize);
						if (ret != 0x00 )
						{
							datasize = -1;
						}	
					}
				}
				else//Faac Encoder
				{
					pAACbuf=m_AACEncoderManager.Encoder(pBuffer,nBufSize,datasize);	
					if(pAACbuf == NULL)
					{
						datasize = -1;
					}	
				}
				if ( datasize>0&&pAACbuf)
				{
					frameinfo.codec = EASY_SDK_AUDIO_CODEC_AAC;
					frameinfo.length = datasize;
					frameinfo.sample_rate = m_sDevConfigInfo.AudioInfo.nSampleRate;
					frameinfo.channels = m_sDevConfigInfo.AudioInfo.nChannaels;
					frameinfo.width  = nVideoWidth;
					frameinfo.height = nVideoHeight;
					frameinfo.fps    = nFps;

					long nTimeStamp = clock();
					frameinfo.timestamp_sec = nTimeStamp/1000;
					frameinfo.timestamp_usec = (nTimeStamp%1000)*1000;
					SourceManager(nDevId, (int*)&m_sSourceInfo, EASY_SDK_AUDIO_FRAME_FLAG, (char*)pAACbuf, &frameinfo);
				}	
			}
		}
		break;
	}
}

int CSourceManager::SourceManager(int _channelId, int *_channelPtr, int _frameType, char *pBuf, RTSP_FRAME_INFO* _frameInfo)
{
	EASY_LOCAL_SOURCE_T *pLocalSource = (EASY_LOCAL_SOURCE_T *)_channelPtr;
	if (NULL == pLocalSource)		return 0;

	EASY_AV_Frame	frame;
	memset(&frame, 0x00, sizeof(EASY_AV_Frame));
	if (NULL != _frameInfo)
	{
		frame.pBuffer = (Easy_U8*)pBuf;
		frame.u32AVFrameLen = _frameInfo->length;
		frame.u32TimestampSec = _frameInfo->timestamp_sec;
		frame.u32TimestampUsec = _frameInfo->timestamp_usec;
		frame.u32VFrameType   = _frameInfo->type;
	}
	if (_frameType == EASY_SDK_VIDEO_RAW_RGB)
	{
		UpdateLocalVideo((unsigned char*)pBuf, _frameInfo->length, _frameInfo->width, _frameInfo->height);
	}
	else if (_frameType == EASY_SDK_VIDEO_FRAME_FLAG)
	{
		BOOL bIsPushing = IsInPushing();
		if(bIsPushing )
		{
			frame.u32AVFrameFlag = EASY_SDK_VIDEO_FRAME_FLAG;
			EasyPusher_PushFrame(m_sPushInfo.pusherHandle, &frame );

			//����RTMP
			if (m_pEasyrtmp)
			{
				bool bKeyFrame = (_frameInfo->type == EASY_SDK_VIDEO_FRAME_I) ?  true : false;
				long nTimeStamp = clock();
				m_pEasyrtmp->WriteVideoH264((unsigned char*)pBuf, _frameInfo->length, nTimeStamp, bKeyFrame);
			}
		}

		bool bKeyFrame  = (_frameInfo->type == 1) ? true : false;
		if (bKeyFrame)
		{
			if (m_bWriteMp4)
			{
				if (!m_bRecording)
				{
					char sFileName[MAX_PATH];
					sprintf(sFileName, "./ThisIsAMP4File_%d.mp4", _channelId );
					CreateMP4Writer(sFileName, _frameInfo->width, _frameInfo->height, _frameInfo->fps, _frameInfo->sample_rate,  _frameInfo->channels, 16, ZOUTFILE_FLAG_FULL, m_bUseGpac);
				}
			}
		}
		if (m_bRecording)
		{
			WriteMP4VideoFrame((unsigned char*)pBuf,  _frameInfo->length, bKeyFrame, clock(), _frameInfo->width, _frameInfo->height);
		}
	}
	else if (_frameType == EASY_SDK_AUDIO_FRAME_FLAG)
	{
		if(IsInPushing())
		{
			frame.u32AVFrameFlag = EASY_SDK_AUDIO_FRAME_FLAG;
			EasyPusher_PushFrame(m_sPushInfo.pusherHandle, &frame );
			//����RTMP
			if (m_pEasyrtmp)
			{
				long nTimeStamp = clock();
				m_pEasyrtmp->WriteAudioAdst((unsigned char*)pBuf, _frameInfo->length,nTimeStamp);
			}
		}
		if (m_bRecording)
		{
			//���AAC������Ƶ����adstͷ����Ҫȥ��ͷ7���ֽڵ�ͷ
			WriteMP4AudioFrame((unsigned char*)pBuf+7,  _frameInfo->length-7, clock());
		}
	}
	else if (_frameType == EASY_SDK_MEDIA_INFO_FLAG)
	{
		memcpy_s(&m_mediainfo, sizeof(m_mediainfo), pBuf, sizeof(EASY_MEDIA_INFO_T));
		if (IsInPushing())
		{
			//ֹͣ����
			StopPush();
			//��ʼ����
			StartPush(m_sPushInfo.pushServerAddr, m_sPushInfo.pushServerPort, m_sPushInfo.sdpName);

		}
// 		pLocalSource->pusherHandle = EasyPusher_Create();
// 		if (NULL != pLocalSource->pusherHandle)
// 		{
// 			EasyPusher_StartStream(pLocalSource->pusherHandle, pLocalSource->pushServerAddr, pLocalSource->pushServerPort, pLocalSource->sdpName+1, "", "", (EASY_MEDIA_INFO_T*)pBuf, 1024, 0);
// 		}
	}
	return 1;
}

//DShow�ɼ���Ƶ��ʾ�ӿ�
void CSourceManager::UpdateLocalVideo(unsigned char *pbuf, int size, int width, int height)
{
	RECT rcClient;
	SetRect(&rcClient, 0, 0, width, height);
	RGB_DrawData(m_d3dHandle, m_hCaptureWnd, (char*)pbuf, width, height, &rcClient, 0x00, RGB(0x00,0x00,0x00), 0x01);
}

int CSourceManager::StartDSCapture(int nCamId, int nAudioId,HWND hShowWnd,int nVideoWidth, int nVideoHeight, int nFps, int nBitRate, char* szDataype, int nSampleRate, int nChannel)
{
	if (m_bDSCapture)
	{
		return 0;
	}
	if (!m_pVFXMaker)
	{
		m_pVFXMaker = Create_VideoVFXMaker();
	}

	if(!m_pVideoManager)
	{
		m_pVideoManager = Create_VideoCapturer();
	}
	if(!m_pAudioManager)
	{
		m_pAudioManager = Create_AudioCapturer();
	}
	//�豸����������Ϣ�ṹ
	memset(&m_sDevConfigInfo, 0x0, sizeof(m_sDevConfigInfo));

	BOOL bUseThread = FALSE;
	int nRet = 0;
	CString strTemp = _T("");
	//�����豸
	// ����������Ӧ���и������������豸����Ϣ [11/3/2015-13:21:06 Dingshuai]
	//GetDevInfoByDevIndex(nI, &DevConfigInfo);

		//1. ������������һ���豸��Ϣ
		m_sDevConfigInfo.nDeviceId = 1;
		m_sDevConfigInfo.nVideoId = nCamId;//�������Ƶ����ID
		m_sDevConfigInfo.nAudioId = nAudioId;//��Ƶ����ID
		m_sDevConfigInfo.VideoInfo.nFrameRate = nFps;
		m_sDevConfigInfo.VideoInfo.nWidth = nVideoWidth;
		m_sDevConfigInfo.VideoInfo.nHeight = nVideoHeight;
		if (szDataype)
		{
			strcpy_s(m_sDevConfigInfo.VideoInfo.strDataType, 64, szDataype);
		}
		else
		{
			strcpy_s(m_sDevConfigInfo.VideoInfo.strDataType, 64, "YUY2");
		}
		m_sDevConfigInfo.VideoInfo.nRenderType = 1;
		m_sDevConfigInfo.VideoInfo.nPinType = 1;
		m_sDevConfigInfo.VideoInfo.nVideoWndId = 0;

		m_sDevConfigInfo.AudioInfo.nAudioBufferType = 4096;
		m_sDevConfigInfo.AudioInfo.nBytesPerSample = 16;
		m_sDevConfigInfo.AudioInfo.nSampleRate = nSampleRate;//44100;//
		m_sDevConfigInfo.AudioInfo.nChannaels = nChannel;
		m_sDevConfigInfo.AudioInfo.nPinType = 2;

		//��ʼ��Pusher�ṹ��Ϣ
		memset(&m_mediainfo, 0x00, sizeof(EASY_MEDIA_INFO_T));
		m_mediainfo.u32VideoCodec =  EASY_SDK_VIDEO_CODEC_H264;//EASY_SDK_VIDEO_CODEC_H264;//0x1C;
		m_mediainfo.u32VideoFps = nFps;
		m_mediainfo.u32AudioCodec = EASY_SDK_AUDIO_CODEC_AAC;
		m_mediainfo.u32AudioChannel = nChannel;
		m_mediainfo.u32AudioSamplerate = nSampleRate;//44100;//
		m_mediainfo.u32AudioBitsPerSample = 16;

		//��ʼ��RGB24->I420ɫ�ʿռ�ת��������ת��ʱ��ѯ�����Ч��
		InitLookupTable();
		//���������ڴ�ռ䣬������������ݵĵ���
		if (!m_EncoderBuffer)
		{
			m_EncoderBuffer = new char[1920*1080];	
		}	

		if (m_bUseFFEncoder)
		{
			//FFEncoder -- Init start
			int	width = nVideoWidth;
			int height = nVideoHeight;
			int fps = nFps;
			int gop = 30;
			int bitrate = nBitRate*1024;//512000; 
			int	intputformat = 0;		//3:rgb24  0:yv12
			CString strDataType = _T("");
			strDataType = m_sDevConfigInfo.VideoInfo.strDataType;
			if (strDataType == _T("RGB24"))
			{
				intputformat = 3;	
			} 
			else
			{
				intputformat = 0;	
			}

			m_nFrameNum = 0;
			//��ʼ��H264������
			FFE_Init(&m_hFfeVideoHandle);	//��ʼ��
			FFE_SetVideoEncodeParam(m_hFfeVideoHandle, ENCODER_H264, width, height, fps, gop, bitrate, intputformat);		//���ñ������
			//��ʼ��AAC������
			AAC_Init(&m_hFfeAudioHandle, nSampleRate/*44100*/, nChannel);
			//FFEncoder -- Init end
			m_mediainfo.u32VideoCodec =  EASY_SDK_VIDEO_CODEC_H264;//EASY_SDK_VIDEO_CODEC_H264;//0x1C;

		} 
		else
		{
			m_AACEncoderManager.Init(nSampleRate, nChannel);
			if(!m_pEncConfigInfo)
				m_pEncConfigInfo = new Encoder_Config_Info;

			m_pEncConfigInfo->nScrVideoWidth = nVideoWidth;
			m_pEncConfigInfo->nScrVideoHeight = nVideoHeight;
			m_pEncConfigInfo->nFps = nFps;
			m_pEncConfigInfo->nMainKeyFrame = 30;
			m_pEncConfigInfo->nMainBitRate = nBitRate;
			m_pEncConfigInfo->nMainEncLevel = 1;
			m_pEncConfigInfo->nMainEncQuality = 28;
			m_pEncConfigInfo->nMainUseQuality = 0;
			if (m_nEncoderType == 1)//h265
			{
				m_H265Encoder.Init(m_pEncConfigInfo->nScrVideoWidth,
					m_pEncConfigInfo->nScrVideoHeight, m_pEncConfigInfo->nFps, m_pEncConfigInfo->nMainKeyFrame,
					m_pEncConfigInfo->nMainBitRate, m_pEncConfigInfo->nMainEncLevel,
					m_pEncConfigInfo->nMainEncQuality, m_pEncConfigInfo->nMainUseQuality);
				m_H265Encoder.GetVPSSPSAndPPS(m_vps, m_vpslen, m_sps, m_spslen, m_pps, m_ppslen);
				if (m_vps && m_vpslen>0)
				{
					memcpy(m_mediainfo.u8Vps, m_vps, m_vpslen);
					m_mediainfo.u32VpsLength = m_vpslen;
				}
				if (m_sps && m_spslen>0)
				{
					memcpy(m_mediainfo.u8Sps, m_sps, m_spslen);
					m_mediainfo.u32SpsLength = m_spslen;
				}
				if (m_pps && m_ppslen>0)
				{
					memcpy(m_mediainfo.u8Pps, m_pps, m_ppslen);
					m_mediainfo.u32PpsLength = m_ppslen;
				}
				m_mediainfo.u32VideoCodec =  EASY_SDK_VIDEO_CODEC_H265;//EASY_SDK_VIDEO_CODEC_H264;//0x1C;
			}
			else //h264
			{
				//x264+faac Encoder --- Init Start

				m_H264EncoderManager.Init(0,m_pEncConfigInfo->nScrVideoWidth,
					m_pEncConfigInfo->nScrVideoHeight,m_pEncConfigInfo->nFps,m_pEncConfigInfo->nMainKeyFrame,
					m_pEncConfigInfo->nMainBitRate,m_pEncConfigInfo->nMainEncLevel,
					m_pEncConfigInfo->nMainEncQuality,m_pEncConfigInfo->nMainUseQuality);

				byte  sps[100];
				byte  pps[100];
				long spslen=0;
				long ppslen=0;
				m_H264EncoderManager.GetSPSAndPPS(0,sps,spslen,pps,ppslen);
				memcpy(m_sps, sps,100) ;
				memcpy(m_pps, pps,100) ;
				m_spslen = spslen;
				m_ppslen = ppslen;
				//x264+faac Encoder --- Init End
// 				if (m_sps && m_spslen>0)
// 				{
// 					memcpy(m_mediainfo.u8Sps, m_sps, m_spslen);
// 					m_mediainfo.u32SpsLength = m_spslen;
// 				}
// 				if (m_pps && m_ppslen>0)
// 				{
// 					memcpy(m_mediainfo.u8Pps, m_pps, m_ppslen);
// 					m_mediainfo.u32PpsLength = m_ppslen;
// 				}
				m_mediainfo.u32VideoCodec =  EASY_SDK_VIDEO_CODEC_H264;//EASY_SDK_VIDEO_CODEC_H264;//0x1C;
			}
		}

		//��Ƶ����
		if (m_sDevConfigInfo.nVideoId >= 0)
		{
			HWND hWnd = hShowWnd;		
			// 2.������Ƶ��ȡ��ʾ����
			m_pVideoManager->SetVideoCaptureData(0, m_sDevConfigInfo.nVideoId,
				hWnd,
				m_sDevConfigInfo.VideoInfo.nFrameRate,  m_sDevConfigInfo.VideoInfo.nWidth,
				m_sDevConfigInfo.VideoInfo.nHeight,     m_sDevConfigInfo.VideoInfo.strDataType, 
				m_sDevConfigInfo.VideoInfo.nRenderType, m_sDevConfigInfo.VideoInfo.nPinType, 1, bUseThread);

			m_pVideoManager->SetDShowCaptureCallback((RealDataCallback)(CSourceManager::RealDataCallbackFunc), (void*)/*s_pSourceManager*/this);

			// 3.������ȡ��Ƶ��ͼ��
			nRet =m_pVideoManager->CreateCaptureGraph();
			if(nRet<=0)
			{
				m_pVideoManager->SetCaptureVideoErr(nRet);
				Release_VideoCapturer(m_pVideoManager);
				m_pVideoManager = NULL;

				strTemp.Format(_T("Video[%d]--����������·ʧ��--In StartDSCapture()."), nCamId);
				LogErr(strTemp);
				return -1;
			}
			nRet = m_pVideoManager->BulidPrivewGraph();
			if(nRet<0)
			{
				Release_VideoCapturer(m_pVideoManager);
				m_pVideoManager = NULL;

				strTemp.Format(_T("Video[%d]--������·ʧ��--In StartDSCapture()."), nCamId);			
				LogErr(strTemp);
				return -1;
			}
			else
			{
				if (nRet == 2)
				{
					strTemp.Format(_T("Video[%d]--���豸��֧���ڲ����ԣ��������ⲿ����ģʽ��(��������Ϊû�п��Խ��л��Ƶı���)--In StartDSCapture()."), nCamId);			
					LogErr(strTemp);
				}
				m_pVideoManager->BegineCaptureThread();
			}
		}
		else
		{
			Release_VideoCapturer(m_pVideoManager)	;
			m_pVideoManager = NULL;
			LogErr(_T("��ǰ��Ƶ�豸������!"));
		}

	//��Ƶ����
	if (m_sDevConfigInfo.nAudioId >= 0)
	{
		m_pAudioManager->SetAudioCaptureData(m_sDevConfigInfo.nAudioId, m_sDevConfigInfo.AudioInfo.nChannaels, 
			m_sDevConfigInfo.AudioInfo.nBytesPerSample,  m_sDevConfigInfo.AudioInfo.nSampleRate, 
			m_sDevConfigInfo.AudioInfo.nAudioBufferType, m_sDevConfigInfo.AudioInfo.nPinType, 2, bUseThread);

		m_pAudioManager->SetDShowCaptureCallback((RealDataCallback)(CSourceManager::RealDataCallbackFunc), (void*)this);

		nRet =m_pAudioManager->CreateCaptureGraph();
		if(nRet<=0)
		{
			strTemp.Format(_T("Audio[%d]--����������·ʧ��--In StartDSCapture()."), nAudioId);
			LogErr(strTemp);

			Release_AudioCapturer(m_pAudioManager);
			m_pAudioManager = NULL;
			return -2;
		}
		if (m_pAudioManager)
		{
			nRet = m_pAudioManager->BulidCaptureGraph();
			if(nRet<0)
			{
				strTemp.Format(_T("Audio[%d]--������·ʧ��--In StartDSCapture()."), nAudioId);
				LogErr(strTemp);

				Release_AudioCapturer(m_pAudioManager);
				m_pAudioManager = NULL;
				return -2;
			}
			else
			{
				m_pAudioManager->BegineCaptureThread();	
			}
		}
	}	
	else
	{
		LogErr(_T("��ǰ��Ƶ�豸������!"));
	}
	return nRet;
}

//�ļ�Դ�ɼ�
int CSourceManager::InitFileCapture(char* sFilePath, int nStartTime, int nEndTime, bool bAutoLoop)
{
	if(!m_pEasyFileCapture)
	{
		m_pEasyFileCapture = new CEasyFileCapture();
		m_pEasyFileCapture->SetCaptureFileCallback( (CaptureFileCallback)&CSourceManager::CaptureFileCallBack, this);
	}
	m_pEasyFileCapture->InitFileCapture(sFilePath, nStartTime, nEndTime,  bAutoLoop);

	//��ʼ��Pusher�ṹ��Ϣ
 	memset(&m_mediainfo, 0x00, sizeof(EASY_MEDIA_INFO_T));
	m_pEasyFileCapture->GetMediaInfo(m_mediainfo);

// 	m_mediainfo.u32VideoCodec =  EASY_SDK_VIDEO_CODEC_H264;//0x1C;
// 	m_mediainfo.u32VideoFps = tempMediaInfo.nFps;
// 
// 	m_mediainfo.u32AudioCodec = EASY_SDK_AUDIO_CODEC_AAC;
// 	int nChannel = 2;
// 	int nSampleRate = 44100;
// 	if (tempMediaInfo.nChannels>0)
// 	{
// 		nChannel = tempMediaInfo.nChannels;
// 	} 
// 	if (tempMediaInfo.nAudioSampleRate>0)
// 	{
// 		nSampleRate = tempMediaInfo.nAudioSampleRate;
// 	} 
// 	m_mediainfo.u32AudioChannel = nChannel;
// 	m_mediainfo.u32AudioSamplerate = nSampleRate;//44100;//

	return 1;
}

void CSourceManager::StartFileCapture()
{
	if (m_pEasyFileCapture)
	{
		m_pEasyFileCapture->StartFileCapture();
	}
}

//��ʼ����(�ɼ�)
// eSourceType==SOURCE_LOCAL_CAMERAʱ��nCamId��Ч
// eSourceType==SOURCE_RTSP_STREAM/SOURCE_ONVIF_STREAMʱ��szURL��Ч
int CSourceManager::StartCapture(SOURCE_TYPE eSourceType, int nCamId, int nAudioId, HWND hCapWnd, char* szFilePath, int nStartTime, int nEndTime, bool bAutoLoop,
	char* szURL, int nVideoWidth, int nVideoHeight, int nFps, int nBitRate, char* szDataType,  int nSampleRate, int nChannel )
{
	if (IsInCapture())
	{
		LogErr(_T("�ɼ����ڽ�����..."));
		//StopCapture();
		return -1;
	}
	int nRet = 1;
	m_hCaptureWnd = hCapWnd;
	//RTSP Source
	if (eSourceType==SOURCE_LOCAL_CAMERA || eSourceType==SOURCE_SCREEN_CAPTURE )
	{
		//DShow���زɼ�/��Ļ�ɼ�
		nRet = StartDSCapture(nCamId, nAudioId, m_hCaptureWnd, nVideoWidth, nVideoHeight, nFps, nBitRate,  szDataType, nSampleRate, nChannel);	
	}
	else if (SOURCE_FILE_STREAM == eSourceType)
	{
		//�ļ�Դ�ɼ�
		nRet = InitFileCapture(szFilePath, nStartTime, nEndTime, bAutoLoop);
	}
	else
	{
		memset(m_sSourceInfo.pushServerAddr, 0x00, sizeof(m_sSourceInfo.pushServerAddr));
		m_sSourceInfo.pushServerPort = 0;
		memset(m_sSourceInfo.sdpName, 0x00, sizeof(m_sSourceInfo.sdpName));
		char	pushHead[128]  = {0,};
		strcpy(pushHead,szURL+7);//strlen(szURL)
		sscanf(pushHead, "%[-_a-zA-Z0-9.]:%d%[-_a-zA-Z0-9:/.]", m_sSourceInfo.pushServerAddr, &m_sSourceInfo.pushServerPort, m_sSourceInfo.sdpName);

		m_sSourceInfo.pMaster = this;
		m_sSourceInfo.rtspSourceId = m_netStreamCapture.Start(szURL, hCapWnd, DISPLAY_FORMAT_RGB24_GDI, 0x01, "", "", &CSourceManager::__MediaSourceCallBack, (void *)&m_sSourceInfo);
		m_netStreamCapture.Config(3, FALSE, TRUE);
		if (m_sSourceInfo.rtspSourceId<=0)
		{
			LogErr(_T("������Ƶ���ɼ�ʧ�ܣ�"));
			return -1;
		}		
	}
	m_bDSCapture = TRUE;
	
	return nRet;
}

//ֹͣ�ɼ�
void CSourceManager::StopCapture()
{
	//Stop Capture
// 	m_videoCamera.CloseCamera();
// 	m_audioCapture.Stop();
	
	CloseMP4Writer();

	//ֹͣ�ļ�����
	if(m_pEasyFileCapture)
	{
		m_pEasyFileCapture->StopFileCapture();
		delete m_pEasyFileCapture;
		m_pEasyFileCapture = NULL;
	}

	//������ڹ����豸
	if (m_pVideoManager)
	{
		Release_VideoCapturer(m_pVideoManager);
		m_pVideoManager = NULL;
	}
	if (m_pAudioManager)
	{
		Release_AudioCapturer(m_pAudioManager);
		m_pAudioManager = NULL;
	}
	m_netStreamCapture.Close();
	
	if (m_hFfeVideoHandle)
	{
		FFE_Deinit(&m_hFfeVideoHandle);
		m_hFfeVideoHandle = NULL;
	}
	if (m_hFfeAudioHandle)
	{
		AAC_Deinit(&m_hFfeAudioHandle);
		m_hFfeAudioHandle = NULL;
	}

	if (m_pEncConfigInfo)
	{
		delete m_pEncConfigInfo;
		m_pEncConfigInfo = NULL;
	}
	m_H264EncoderManager.Clean();
	m_AACEncoderManager.Clean();
	m_H265Encoder.Clean();

	m_nFrameNum = 0;
	if (m_EncoderBuffer)
	{
		delete[] m_EncoderBuffer ;	//���������ڴ�ռ�
		m_EncoderBuffer = NULL;
	}	
	//����VFX MAKER
	if (m_pVFXMaker)
	{
		Release_VideoVFXMaker(m_pVFXMaker);
		m_pVFXMaker = NULL;
	}

	m_bDSCapture = FALSE;
}

int __EasyPusher_Callback(int _id, EASY_PUSH_STATE_T _state, EASY_AV_Frame *_frame, void *_userptr)
{
	if (_state == EASY_PUSH_STATE_CONNECTING)               TRACE("Connecting...\n");
	else if (_state == EASY_PUSH_STATE_CONNECTED)           TRACE("Connected\n");
	else if (_state == EASY_PUSH_STATE_CONNECT_FAILED)      TRACE("Connect failed\n");
	else if (_state == EASY_PUSH_STATE_CONNECT_ABORT)       TRACE("Connect abort\n");
	else if (_state == EASY_PUSH_STATE_PUSHING)             TRACE("P->");
	else if (_state == EASY_PUSH_STATE_DISCONNECTED)        TRACE("Disconnect.\n");

	return 0;
}

//��ʼ����
int CSourceManager::StartPush(char* ServerIp, int nPushPort, char* sPushName, int nPushBufSize, bool bPushRtmp)
{
	m_sPushInfo.pusherHandle = EasyPusher_Create();
	strcpy(m_sPushInfo.pushServerAddr,  ServerIp);
	m_sPushInfo.pushServerPort = nPushPort;
	strcpy(m_sPushInfo.sdpName, sPushName);
	Easy_U32 nRet = 0;
	if (NULL != m_sPushInfo.pusherHandle )
	{
		EasyPusher_SetEventCallback(m_sPushInfo.pusherHandle, __EasyPusher_Callback, 0, NULL);
		nRet = EasyPusher_StartStream(m_sPushInfo.pusherHandle , 
			ServerIp, nPushPort, sPushName, EASY_RTP_OVER_TCP, "", "", (EASY_MEDIA_INFO_T*)&m_mediainfo, nPushBufSize, 0);//512-2048
	}	
	else
	{
		nRet = -1;
	}
	m_bPushRtmp = bPushRtmp;
	if (bPushRtmp)
	{
		if (!m_pEasyrtmp)
		{
			m_pEasyrtmp=new EasyRtmp();
		}
		char szURL[MAX_PATH];
		memset(szURL, 0, sizeof(MAX_PATH));
		sprintf(szURL, "rtmp://%s:1935/live/%s", ServerIp,  sPushName);
		nRet = m_pEasyrtmp->Link(szURL,1280);
	}

	if(nRet>=0)
	{
		m_bPushing = TRUE;
	}
	else
	{
		StopPush();
	}
	return nRet;
}

//ֹͣ����
void CSourceManager::StopPush()
{
	m_bPushing = FALSE;
	//Close Pusher
	if (NULL != m_sPushInfo.pusherHandle)
	{
		EasyPusher_StopStream(m_sPushInfo.pusherHandle);
		EasyPusher_Release(m_sPushInfo.pusherHandle);
		m_sPushInfo.pusherHandle = NULL;
	}
	if (m_pEasyrtmp)
	{
		m_pEasyrtmp->Clean();
		delete m_pEasyrtmp;
		m_pEasyrtmp=NULL;
	}
}

//��ʼ����
int CSourceManager::StartPlay(char* szURL, HWND hShowWnd)
{
	m_sPlayInfo.rtspSourceId = m_netStreamPlayer.Start(szURL, hShowWnd, DISPLAY_FORMAT_RGB24_GDI, 0x01, "", "");
	m_netStreamPlayer.Config(3, TRUE, TRUE, TRUE);
	return m_sPlayInfo.rtspSourceId ;
}

//ֹͣ����
void CSourceManager::StopPlay()
{
	m_netStreamPlayer.Close();
}

void CSourceManager::LogErr(CString strLog)
{
	if(!strLog.IsEmpty())
	{
		TCHAR* szLog = new TCHAR[strLog.GetLength()+1];
		StrCpy(szLog, strLog);
		if(m_pMainDlg)
			m_pMainDlg->SendMessage(MSG_LOG, 0, (LPARAM)szLog);

		delete[] szLog;
		szLog = NULL;
	}
}

void CSourceManager::SetMainDlg(	CEasyPusherDlg* pMainDlg)
{
	m_pMainDlg = pMainDlg;
}

void CSourceManager::EnumLocalAVDevInfo(CWnd* pComboxMediaSource, CWnd* pComboxAudioSource)
{
	if (NULL != pComboxMediaSource)
	{
		CComboBox * pVideoSource = (CComboBox *)pComboxMediaSource;
		CAMERA_LIST_T *pUSBCameraList = m_videoCamera.GetCameraList();
		CAMERA_INFO_T	*pCameraInfo = pUSBCameraList->pCamera;
		if (NULL != pCameraInfo)
		{
			while (pCameraInfo)
			{
				wchar_t wszCameraName[64] = {0,};
				__MByteToWChar(pCameraInfo->friendlyName, wszCameraName, sizeof(wszCameraName)/sizeof(wszCameraName[0]));

				pVideoSource->AddString(wszCameraName);
				//CAMERA_INFO_T	*pCameraInfo
				pCameraInfo = pCameraInfo->pNext;
			}
		}
		pVideoSource->SetCurSel(0);
	}

	if (NULL != pComboxAudioSource)
	{
		m_audioCapture.GetAudioInputDevices(pComboxAudioSource->GetSafeHwnd());
	}

}

void CSourceManager::ResizeVideoWnd()
{
	if (m_pVideoManager)
	{
		m_pVideoManager->ResizeVideoWindow();
	} 
}

//��Ļ�ɼ�
int CSourceManager::StartScreenCapture(HWND hShowWnd, int nCapMode)
{
	if (!m_pScreenCaptrue)
	{
		//ʵ������Ļ���������ָ��
		m_pScreenCaptrue =  CCaptureScreen::Instance(m_nScreenCaptureId);
		if (!m_pScreenCaptrue)
		{
			return -1;
		}
		m_pScreenCaptrue->SetCaptureScreenCallback((CaptureScreenCallback)&CSourceManager::CaptureScreenCallBack, this);
	}
	if (m_pScreenCaptrue->IsInCapturing())
	{
		return -1;
	}
	return m_pScreenCaptrue->StartScreenCapture(hShowWnd, nCapMode);

#if 0
	// for a D3DCaptureScreen test [4/15/2016 Administrator]
	if (!m_pD3dScreenCaptrue)
	{
		m_pD3dScreenCaptrue = new CD3DCaptureScreem();
		m_pD3dScreenCaptrue->SetCaptureScreenCallback((CaptureScreenCallback)&CSourceManager::CaptureScreenCallBack, this);
	}
	if (m_pD3dScreenCaptrue->IsInCapturing())
	{
		return -1;
	}
	m_pD3dScreenCaptrue->InitD3DCapture(hShowWnd);
#endif

	return 1;
}
void CSourceManager::StopScreenCapture()
{
	if (m_pScreenCaptrue)
	{
		if (m_pScreenCaptrue->IsInCapturing())
		{
			m_pScreenCaptrue->StopScreenCapture();
		}
	}
	if (m_pD3dScreenCaptrue)
	{
		if (m_pD3dScreenCaptrue->IsInCapturing())
		{
			m_pD3dScreenCaptrue->StopD3DScreenCapture();
		}
	}
}

void CSourceManager::RealseScreenCapture()
{
	if (m_pScreenCaptrue)
	{
		CCaptureScreen::UnInstance(m_nScreenCaptureId);
		m_pScreenCaptrue = NULL;
	}
	if (m_pD3dScreenCaptrue)
	{
		m_pD3dScreenCaptrue->StopD3DScreenCapture();
		delete m_pD3dScreenCaptrue;
		m_pD3dScreenCaptrue = NULL;
	}
}

int CSourceManager::GetScreenCapSize(int& nWidth, int& nHeight)
{
	if (m_pScreenCaptrue)
	{
		if (m_pScreenCaptrue->IsInCapturing())
		{
			m_pScreenCaptrue->GetCaptureScreenSize(nWidth, nHeight );
			return 1;
		}
		else
			return -1;
	}
	else if (m_pD3dScreenCaptrue)
	{
		m_pD3dScreenCaptrue->GetCaptureScreenSize(nWidth, nHeight);
		return 1;
	}
	return -1;
}

// 	//��ʼ�����
// 	int	LIB_MP4CREATOR_API	MP4C_Init(MP4C_Handler *handler);
// 	//����¼���ļ�
// 	int	LIB_MP4CREATOR_API	MP4C_CreateMp4File(MP4C_Handler handler, char *filename, unsigned int _file_buf_size/*�ڴ滺��,����������֮��Ż�д���ļ�, ���Ϊ0��ֱ��д���ļ�*/);
// 	//������Ƶ����
// 	int	LIB_MP4CREATOR_API	MP4C_SetMp4VideoInfo(MP4C_Handler handler, unsigned int codec,	unsigned short width, unsigned short height, unsigned int fps);
// 	//������Ƶ����
// 	int	LIB_MP4CREATOR_API	MP4C_SetMp4AudioInfo(MP4C_Handler handler, unsigned int codec,	unsigned int sampleFrequency, unsigned int channel);
// 	//����H264�е�SPS
// 	int	LIB_MP4CREATOR_API	MP4C_SetH264Sps(MP4C_Handler handler, unsigned short sps_len, unsigned char *sps);
// 	//����H264�е�PPS
// 	int	LIB_MP4CREATOR_API	MP4C_SetH264Pps(MP4C_Handler handler, unsigned short pps_len, unsigned char *pps);
// 
// 	//��֡��������ȡSPS��PPS,��ȡ��������MP4C_SetH264Sps��MP4C_SetH264Pps
// 	//֡����������� start code
// 	int LIB_MP4CREATOR_API  MP4C_GetSPSPPS(char *pbuf, int bufsize, char *_sps, int *_spslen, char *_pps, int *_ppslen);
// 
// 	/*
// 	д��ý������
// 	//������������Ƶ����Ƶ, ֱ�ӵ���MP4C_AddFrameд�뼴��, ���ڲ�������ж�,��д��һ��GOP��д���Ӧʱ��ε���Ƶ
// 	//����Ƶ��֧��AAC   8KHz ��  44.1KHz
// 	pbuf������start code 00 00 00 01Ҳ����û��,�����������ж�
// 	���û��start code, �������MP4C_SetH264Sps��MP4C_SetH264Pps������Ӧ��sps��pps
// 
// 	֡����Ϊ���¼������ʱ�ɲ�����MP4C_SetH264Sps��MP4C_SetH264Pps:
// 	1.  start code + sps + start code + pps + start code + idr
// 	2.  start code + sps            start code + pps   ��start code+spsΪһ֡, start code+ppsΪһ֡
// 	*/
// 	int	LIB_MP4CREATOR_API	MP4C_AddFrame(MP4C_Handler handler, unsigned int mediatype, unsigned char *pbuf, unsigned int framesize, unsigned char keyframe, unsigned int timestamp_sec, unsigned int timestamp_rtp, unsigned int fps);
// 
// 	//�ر�MP4�ļ�, �����ļ���С
// 	unsigned int LIB_MP4CREATOR_API	MP4C_CloseMp4File(MP4C_Handler handler);
// 
// 	//�ͷž��
// 	int	LIB_MP4CREATOR_API	MP4C_Deinit(MP4C_Handler *handler);
//дMP4�ļ�(¼�����)
int CSourceManager::CreateMP4Writer(char* sFileName, int nVWidth, int nVHeight, int nFPS, int nSampleRate, int nChannel, int nBitsPerSample, int nFlag, BOOL bUseGpac)
{
	if (m_bRecording)
	{
		return -1;
	}
	m_bUseGpac = bUseGpac;
	if (!bUseGpac)
	{
		if (m_handler)
		{
			return -1;
		}
		MP4C_Init(&m_handler);
		MP4C_SetMp4VideoInfo(m_handler, VIDEO_CODEC_H264, nVWidth, nVHeight, nFPS);
		MP4C_SetMp4AudioInfo(m_handler, AUDIO_CODEC_AAC, nSampleRate, nChannel );//44100  2
		MP4C_CreateMp4File(m_handler, sFileName, 0);
	} 
	else
	{
		if (!m_pMP4Writer)
		{
			m_pMP4Writer = new EasyMP4Writer();
		}
		if (!m_pMP4Writer->CreateFile(sFileName, nFlag))
		{
			delete m_pMP4Writer;
			m_pMP4Writer = NULL;
			return -1;
		}		

		// 		ǰ����ֽ�Ϊ AAC object types  LOW          2
		// 		����4���ֽ�Ϊ ����index        16000        8
		// 		������־��׼��
		//	static unsigned long tnsSupportedSamplingRates[13] = //��Ƶ�����ʱ�׼����־�����±�Ϊд���־
		//	{ 96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,0 };

		// 		����4���ֽ�Ϊ channels ����                 2
		// 		Ӧ��ӡ������ȷ2������ʽΪ  00010 | 1000 | 0010 | 000
		// 														2        8         2
		//  BYTE ubDecInfoBuff[] =  {0x12,0x10};//00010 0100 0010 000
  
		//��Ƶ�����ʱ�׼����־�����±�Ϊд���־
		unsigned long tnsSupportedSamplingRates[13] = { 96000,88200,64000,48000,44100,32000,24000,22050,16000,12000,11025,8000,0 };
		int nI = 0;
		for ( nI = 0; nI<13; nI++)
		{
			if (tnsSupportedSamplingRates[nI] == nSampleRate )
			{
				break;
			}
		}
		BYTE ucDecInfoBuff[2] = {0x12,0x10};//
		
		unsigned short  nDecInfo = (1<<12) | (nI << 7) | (nChannel<<3);
		int nSize = sizeof(unsigned short);
		memcpy(ucDecInfoBuff, &nDecInfo, nSize);
		SWAP(ucDecInfoBuff[0], ucDecInfoBuff[1]);
		int unBuffSize = sizeof(ucDecInfoBuff)*sizeof(BYTE);

		m_pMP4Writer->WriteAACInfo(ucDecInfoBuff,unBuffSize, nSampleRate, nChannel, nBitsPerSample);
	}
	m_bRecording = 1;

	return 1;
}

int CSourceManager::WriteMP4VideoFrame(unsigned char* pdata, int datasize, bool keyframe, long nTimestamp, int nWidth, int nHeight)
{
	if (!m_bUseGpac)
	{
		MP4C_AddFrame( m_handler, MEDIA_TYPE_VIDEO,pdata , datasize, keyframe, nTimestamp/1000, nTimestamp, 25);
	}
	else
	{
		if (m_pMP4Writer)
		{
			m_pMP4Writer->WriteMp4File(pdata, datasize,keyframe, nTimestamp, nWidth, nHeight  );
		}
	}

	return 1;
}

int CSourceManager::WriteMP4AudioFrame(unsigned char* pdata,int datasize, long timestamp)
{
	if (!m_bUseGpac)
	{
		MP4C_AddFrame( m_handler, MEDIA_TYPE_AUDIO,pdata , datasize, 1, timestamp/1000, timestamp, 25);
	} 
	else
	{
		if (m_pMP4Writer)
		{
			if (m_pMP4Writer->CanWrite())
			{
				return m_pMP4Writer->WriteAACFrame(pdata,datasize, timestamp);
			}
		}
	}

	return 0;
}
void CSourceManager::CloseMP4Writer()
{
	m_bRecording = 0;
	if (!m_bUseGpac)
	{
		if (m_handler)
		{
			MP4C_CloseMp4File(m_handler);
			Sleep(500);
			MP4C_Deinit(&m_handler);
			m_handler = NULL;
		}
	} 
	else
	{
		if (m_pMP4Writer)
		{
			m_pMP4Writer->SaveFile();
			delete m_pMP4Writer;
			m_pMP4Writer=NULL;
		}
	}
}

//������Ļ����
int   CSourceManager::SetOSD (int bIsUse, int nMoveType, int R, int G, int B,
	int weight, int x, int y, int width, int height, char* fontname, char* tittleContent)
{
	m_vfxConfigInfo.tittleInfo.nState = bIsUse;
	m_vfxConfigInfo.tittleInfo.nMoveType = nMoveType;
	m_vfxConfigInfo.tittleInfo.nColorR = R;
	m_vfxConfigInfo.tittleInfo.nColorG = G;
	m_vfxConfigInfo.tittleInfo.nColorB = B;
	m_vfxConfigInfo.tittleInfo.nFontWeight = weight;
	m_vfxConfigInfo.tittleInfo.nTittleWidth = width;
	m_vfxConfigInfo.tittleInfo.nTittleHeight = height;
	m_vfxConfigInfo.tittleInfo.ptStartPosition.x = x;
	m_vfxConfigInfo.tittleInfo.ptStartPosition.y = y;

	strcpy(m_vfxConfigInfo.tittleInfo.strFontType,fontname);
	strcpy(m_vfxConfigInfo.tittleInfo.strTittleContent,tittleContent);

	m_vfxConfigInfo.tittleInfo.bResetTittleInfo = TRUE;
	return 1;
}

//����̨�ꡣlogo��ˮӡ...
int   CSourceManager::SetLogo (int bIsUse, int ePos, int eStyle, 
	int x, int y, int width, int height, char* logopath)
{
	m_vfxConfigInfo.warkMarkInfo.bIsUseWaterMark = bIsUse;
	m_vfxConfigInfo.warkMarkInfo.eWaterMarkPos = (WATER_MARK_POS)ePos;
	m_vfxConfigInfo.warkMarkInfo.eWatermarkStyle = (WATERMARK_ENTRY_TYPE)eStyle;
	m_vfxConfigInfo.warkMarkInfo.nLeftTopX = x;
	m_vfxConfigInfo.warkMarkInfo.nLeftTopY = y;
	m_vfxConfigInfo.warkMarkInfo.nWidth = width;
	m_vfxConfigInfo.warkMarkInfo.nHeight = height;

	strcpy(m_vfxConfigInfo.warkMarkInfo.strWMFilePath, (logopath));
	m_vfxConfigInfo.warkMarkInfo.bResetWaterMark = TRUE;

	return 1;
}


