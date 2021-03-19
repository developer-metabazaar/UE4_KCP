// Fill out your copyright notice in the Description page of Project Settings.


#include "kcpServer.h"
#include "UdpServerv1.h"
#include "RunnableThreadx.h"
#include "MyBlueprintFunctionLibrary.h"

//#define UTF16
TMap<void*, FOnRawsend> kcpServer::onrawsendevents;
kcpServer::kcpServer(UINT16 port, int& iResultp)
{
	kcp1 = ikcp_create(5598781, (void*)this);

	int mode = 2;
	// �жϲ���������ģʽ
	if (mode == 0) {
		// Ĭ��ģʽ
		ikcp_nodelay(kcp1, 1, 20, 2, 1);
		ikcp_wndsize(kcp1, 64, 64);
		ikcp_setmtu(kcp1, 512);
	}
	else if (mode == 1) {
		// ��ͨģʽ���ر����ص�
		ikcp_nodelay(kcp1, 0, 10, 0, 1);
	}
	else {
		// ��������ģʽ
		// �ڶ������� nodelay-�����Ժ����ɳ�����ٽ�����
		// ���������� intervalΪ�ڲ�����ʱ�ӣ�Ĭ������Ϊ 10ms
		// ���ĸ����� resendΪ�����ش�ָ�꣬����Ϊ2
		// ��������� Ϊ�Ƿ���ó������أ������ֹ
		ikcp_nodelay(kcp1,1,10, 2, 1);
		ikcp_wndsize(kcp1, 200, 200);
		ikcp_setmtu(kcp1, 512);
	}
	us = MakeShareable(new UdpServerv1(port, iResultp, [=](char* data, UINT16& size) {
		ikcp_input(kcp1, (const char*)data, size);
		}));
	kcp1->output = [](const char* buf, int len, struct IKCPCB* kcp, void* user)->int {
		FOnRawsend* orp = onrawsendevents.Find(user);
		if (orp)
		{
			orp->ExecuteIfBound(buf, len, kcp, user);
		}
		return 0;
	};
	onrawsendevents.FindOrAdd(this).BindLambda([=](const char* buf, int len, struct IKCPCB* kcp, void* user) {
		int32 BytesSent = 0;
		us->send((char*)buf, len);
		//GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, FString::FromInt(len).Append(" :SenderSocket->SendTo"));
	});
	receivethread = new RunnableThreadx([=]() {
		FPlatformProcess::Sleep(0.015);
		ReceiveWork();
	});
}
kcpServer::~kcpServer()
{
	if (receivethread)
	{
		delete receivethread;
		receivethread = nullptr;
	}
	onrawsendevents.Remove(this);
	//if (kcp1)
	//{
	//	ikcp_release(kcp1);
	//	kcp1 = nullptr;
	//}
}
void kcpServer::close()
{
	if (receivethread)
	{
		delete receivethread;
		receivethread = nullptr;
	}
}
#define TIMEINTERNAL  0.015f
#define TIMEINTERNALINMILLISECOND  TIMEINTERNAL*1000*1
void kcpServer::ReceiveWork()
{
	static int tickcounter = 0;
	currenttime = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
	tickcounter += TIMEINTERNALINMILLISECOND;
	currenttime += tickcounter;
	if (currenttime >= nexttimecallupdate)
	{
		ikcp_update(kcp1, currenttime);
		nexttimecallupdate = ikcp_check(kcp1, currenttime);
		tickcounter = 0;
	}
	//ikcp_update(kcp1, FDateTime::UtcNow().ToUnixTimestamp() * 100);
	int hr = ikcp_recv(kcp1, (char*)kcpreceive, sizeof(kcpreceive));
	if (hr > 0)
	{
		OnkcpReceiveddata.ExecuteIfBound(kcpreceive, hr);
	}
}
void kcpServer::kcpsend(const uint8* buffer, int len)
{
	ikcp_send(kcp1, (const char*)buffer, len);
}
void kcpServer::kcpsend(const FString& msg)
{
#ifdef UTF16
	uint8* pointer;
	int64 outsize;
	UMyBlueprintFunctionLibrary::FStringtoUTF16((FString&)msg, pointer, outsize);
	kcpsend((const uint8*)pointer, outsize);
#else
	const TCHAR* serializedChar = msg.GetCharArray().GetData();
	int32 size = FCString::Strlen(serializedChar);
	//pointer = (uint8*)TCHAR_TO_UTF8(serializedChar);
	kcpsend((const uint8*)TCHAR_TO_UTF8(serializedChar), size);

#endif // SENDUTF16
}
