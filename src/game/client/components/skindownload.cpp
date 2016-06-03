
#include <engine/graphics.h>
#include <engine/textrender.h>

#include "skins.h"
#include "skindownload.h"

void CSkinDownload::OnConsoleInit()
{
	Console()->Register("fetch_skin", "s[skinname]", CFGFLAG_CLIENT, ConFetchSkin, this, "Fetch a skin by name (from ddnet's database)");
}

void CSkinDownload::OnInit()
{
	m_pFetcher = Kernel()->RequestInterface<IFetcher>();
	m_pStorage = Kernel()->RequestInterface<IStorageTW>();
}

void CSkinDownload::OnRender()
{
	const float MAX_HEIGHT = m_FetchTasks.empty() ? 0.001f : 5.0f+25.0f+5.0f+20.0f*(float)m_FetchTasks.size();
	static float s_SmoothPos = 0.0f;
	smooth_set(&s_SmoothPos, MAX_HEIGHT, (0.005f/Client()->RenderFrameTime())*23.0f);

	if(s_SmoothPos <= 0.01f)
		return;

	CUIRect Screen = *(UI()->Screen()), Button;
	Graphics()->MapScreen(0, 0, Screen.w, Screen.h);
	Screen.HSplitBottom(s_SmoothPos, 0, &Screen);
	//float dings = clamp(s_SmoothPos/(5.0f+25.0f+5.0f+20.0f*(float)m_FetchTasks.size()), 0.75f, 1.0f);
	Screen.VMargin(Screen.w/3.0f/**ding*/, &Screen);
	//Screen.x = UI()->Screen()->w/2-Screen.w/2;

	Screen.y += 5.0f;
	RenderTools()->DrawUIRect(&Screen, vec4(0,0,0,0.3f), CUI::CORNER_T, 5.0f);

	Screen.VMargin(10.0f, &Screen);
	TextRender()->TextColor(0.8f, 0.8f, 0.8f, 0.8f);
	Screen.HSplitTop(5.0f, 0, &Screen);
	Screen.HSplitTop(15.0f, &Button, &Screen);
	UI()->DoLabelScaled(&Button, Localize("Skin Downloads"), 14.0f, -1);

	for(std::map<CFetchTask*, SkinFetchTask>::iterator it = m_FetchTasks.begin(); it != m_FetchTasks.end(); it++)
	{
		if(it->second.State == CFetchTask::STATE_ERROR)
			TextRender()->TextColor(1,0,0,1);
		if(it->second.State == CFetchTask::STATE_DONE)
			TextRender()->TextColor(0,1,0,1);
		if(it->second.State == CFetchTask::STATE_QUEUED)
			TextRender()->TextColor(1,1,0,1);

		Screen.HSplitTop(5.0f, 0, &Screen);
		Screen.HSplitTop(13.5f, &Button, &Screen);
		UI()->DoLabelScaled(&Button, it->second.SkinName.c_str(), 12.0f, -1);
		TextRender()->TextColor(1,1,1,1);

		if(it->second.State == CFetchTask::STATE_RUNNING)
		{
			Screen.HSplitTop(3.0f, 0, &Screen);
			Screen.HSplitTop(2.0f, &Button, &Screen);
			RenderTools()->DrawUIRect(&Button, vec4(0.2f, 0.2f, 0.8f, 0.9f), 0, 0);
			Button.w *= (float)it->second.Progress/100.0f;
			RenderTools()->DrawUIRect(&Button, vec4(0.4f, 0.4f, 0.9f, 1.0f), 0, 0);
		}

		if(it->second.FinishTime > 0 && time_get() > it->second.FinishTime + 4 * time_freq())
			m_FetchTasks.erase(it->first);
	}

}

void CSkinDownload::ProgressCallback(CFetchTask *pTask, void *pUser)
{
	CSkinDownload *pSelf = (CSkinDownload *)pUser;
	pSelf->m_FetchTasks[pTask].State = pTask->State();
	pSelf->m_FetchTasks[pTask].Progress = pTask->Progress();
}

void CSkinDownload::CompletionCallback(CFetchTask *pTask, void *pUser)
{
	CSkinDownload *pSelf = (CSkinDownload *)pUser;
	const char *dest = pTask->Dest();
	SkinFetchTask *pTaskHandler = &pSelf->m_FetchTasks[pTask];
	pTaskHandler->State = pTask->State();

	if(pTask->State() == CFetchTask::STATE_ERROR)
	{
		pSelf->m_pStorage->RemoveBinaryFile(dest); // delete the empty file dummy
		dbg_msg("skinfetcher/debug", "download failed: '%s'", dest);
		if(pTaskHandler->pDestID)
			*(pTaskHandler->pDestID) = pSelf->GameClient()->m_pSkins->Find("default");
		pSelf->m_FailedTasks.add(pTaskHandler->SkinName);
	}

	if(pTask->State() == CFetchTask::STATE_DONE)
	{
		dbg_msg("skinfetcher/debug", "download finished: '%s'", dest);
		pSelf->GameClient()->m_pSkins->RefreshSkinList(false);
		if(pTaskHandler->pDestID)
			*(pTaskHandler->pDestID) = pSelf->GameClient()->m_pSkins->Find(pTaskHandler->SkinName.c_str());
	}

	pTaskHandler->FinishTime = time_get();

	delete pTask;
}

void CSkinDownload::RequestSkin(int *pDestID, const char *pName)
{
	int DefaultSkin = GameClient()->m_pSkins->Find("default");

	int SkinID = GameClient()->m_pSkins->Find(pName);
	if(SkinID >= 0)
	{
		*pDestID = SkinID;
		return;
	}
	else
		*pDestID = DefaultSkin;

	if(!g_Config.m_ClFetchSkins)
	{
		*pDestID = DefaultSkin;
		return;
	}

	for(int i = 0; i < m_FailedTasks.size(); i++)
		if(m_FailedTasks[i] == std::string(pName))
			return;

	for(std::map<CFetchTask*, SkinFetchTask>::iterator it = m_FetchTasks.begin(); it != m_FetchTasks.end(); it++)
		if(it->second.SkinName == std::string(pName))
			return;

	FetchSkin(pName, pDestID);
}

void CSkinDownload::FetchSkin(const char *pName, int *pDestID)
{
	char aBuf[256], aDestPath[256] = {0}, aFullPath[512] = {0};
	str_format(aBuf, sizeof(aBuf), "https://ddnet.tw/skins/skin/%s.png", pName);

	//dbg_msg("skinfetcher", "fetching file from '%s'", aBuf);
	str_format(aDestPath, sizeof(aDestPath), "downloadedskins/%s.png", pName);
	IOHANDLE f = Storage()->OpenFile(aDestPath, IOFLAG_WRITE, IStorageTW::TYPE_SAVE, aFullPath, sizeof(aFullPath));
	if(f)
	{
		io_close(f);
		//Storage()->RemoveBinaryFile(aFullPath);
	}
	else
	{
		dbg_msg("skinfetcher/error", "cannot write to '%s'", aFullPath);
		return;
	}

	CFetchTask *pTask = new CFetchTask(false);
	SkinFetchTask Task;
	Task.SkinName = std::string(pName);
	Task.Progress = 0;
	Task.FinishTime = -1;
	Task.pDestID = pDestID;
	m_FetchTasks[pTask] = Task;
	m_pFetcher->QueueAdd(pTask, aBuf, aFullPath, -2, this, &CSkinDownload::CompletionCallback, &CSkinDownload::ProgressCallback);
}

void CSkinDownload::ConFetchSkin(IConsole::IResult *pResult, void *pUserData)
{
	((CSkinDownload *)pUserData)->FetchSkin(pResult->GetString(0));
}
