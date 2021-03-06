#include <vector>
#include <algorithm>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/irc.h>
#include <game/client/gameclient.h>
#include <game/client/components/console.h>
#include <game/client/components/menus.h>

#include "luabinding.h"


int CLuaBinding::LuaListdirCallback(const char *name, const char *full_path, int is_dir, int dir_type, void *user)
{
	lua_State *L = (lua_State*)user;
	lua_pushvalue(L, lua_gettop(L)); // duplicate the callback function which is on top of the stack
	lua_pushstring(L, name); // push arg 1 (element name)
	lua_pushstring(L, full_path); // push arg 2 (element path)
	lua_pushboolean(L, is_dir); // push arg 3 (bool indicating whether element is a folder)
	lua_pcall(L, 3, 1, 0);
	int ret = 0;
	if(lua_isnumber(L, -1))
		ret = round_to_int((float)lua_tonumber(L, -1));
	lua_pop(L, 1); // pop return
	return ret;
}

CLuaFile* CLuaBinding::GetLuaFile(lua_State *L)
{
	if(!L)
		return NULL;

	CGameClient *pGameClient = (CGameClient *)CLua::GameClient();
	if(L == pGameClient->m_pGameConsole->m_pStatLuaConsole->m_LuaHandler.m_pLuaState)
	{
		static CLuaFile ConLuaFile(pGameClient->Client()->Lua(), "[console]", false);
		ConLuaFile.m_pLuaState = L;
		ConLuaFile.m_State = CLuaFile::STATE_CONSOLE;
		return &ConLuaFile;
	}

	for(int i = 0; i < pGameClient->Client()->Lua()->GetLuaFiles().size(); i++)
	{
		if(pGameClient->Client()->Lua()->GetLuaFiles()[i]->L() == L)
		{
			return pGameClient->Client()->Lua()->GetLuaFiles()[i];
		}
	}
	return NULL;
}

// global namespace
int CLuaBinding::LuaImport(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	int n = lua_gettop(L);
	if(n != 1)
		return luaL_argerror(L, 1, "expected a string value, got nil");

	char aFilename[512];
	{
		const char *pFilename = luaL_checklstring(L, 1, 0);
		if(!pFilename)
			return false;

		// replace all backslashes with forward slashes
		char aTmp[512];
		{
			str_copy(aTmp, pFilename, sizeof(aTmp));
			for(char *p = aTmp; *p; p++)
				if(*p == '\\')
					*p = '/';
			pFilename = aTmp;
		}
		// sandbox it
		for(; *pFilename && ((*pFilename == '.' && pFilename[1] == '/') || *pFilename == '/'); pFilename++);
		str_copy(aFilename, pFilename, sizeof(aFilename));
		// auto-append the file ending if omitted
		if(str_comp_nocase(aFilename+str_length(aFilename)-4, ".lua") != 0 && str_comp_nocase(aFilename+str_length(aFilename)-4, ".clc") != 0)
			str_append(aFilename, ".lua", sizeof(aFilename)); // assume plain lua files as default
	}

	char aBuf[512];
	// try same folder as the script first as it's the most intuitive thing
	str_copy(aBuf, pLF->GetFilename(), sizeof(aBuf)); // get the whole path
	str_replace_char_rev_num(aBuf, 1, '/', '\0'); // cut off the file name
	str_append(aBuf, "/", sizeof(aBuf)); // re-append the slash
	str_append(aBuf, aFilename, sizeof(aBuf)); // append the wanted file's path
	bool ret = pLF->LoadFile(aBuf, true);

	// try libraries after that
	char aFullPath[512];
	if(!ret)
	{
		// search everywhere
		str_format(aBuf, sizeof(aBuf), "lua/.library/%s", aFilename);
		IOHANDLE tmp = CLua::m_pCGameClient->Storage()->OpenFile(aBuf, IOFLAG_READ, IStorageTW::TYPE_ALL, aFullPath, sizeof(aFullPath));
		if(tmp)
			io_close(tmp);
		else
			str_copy(aFullPath, aBuf, sizeof(aFullPath)); // fall back to lua folder if it fails

		ret = pLF->LoadFile(aFullPath, true);

		if(!ret)
		{
			// try lua base folder if all fails
			if(str_comp_num(aFilename, "lua/", 4) != 0)
				str_format(aBuf, sizeof(aBuf), "lua/%s", aFilename);
			ret = pLF->LoadFile(aBuf, true);
		}
	}

	if(g_Config.m_Debug)
		dbg_msg("Lua/debug", "script '%s' Import(\"%s\") -> %s", pLF->GetFilename(), aFilename, ret ? "successful" : "failed");

	// return some stuff to the script
	lua_pushboolean(L, (int)ret); // success?
	lua_pushstring(L, (const char *)aFullPath); // actual relative path of the loaded file
	return 2;
}

int CLuaBinding::LuaExec(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	int n = lua_gettop(L);
	if(n != 1)
		return luaL_argerror(L, 1, "expected a string value, got nil");

	char aFilename[512];
	{
		const char *pFilename = luaL_checklstring(L, 1, 0);
		if(!pFilename)
			return false;

		// replace all backslashes with forward slashes
		char aTmp[512];
		{
			str_copy(aTmp, pFilename, sizeof(aTmp));
			for(char *p = aTmp; *p; p++)
				if(*p == '\\')
					*p = '/';
			pFilename = aTmp;
		}
		// sandbox it
		for(; *pFilename && ((*pFilename == '.' && pFilename[1] == '/') || *pFilename == '/'); pFilename++);
		str_copy(aFilename, pFilename, sizeof(aFilename));
		// auto-append the file ending if omitted
		if(str_comp_nocase(aFilename+str_length(aFilename)-4, ".lua") != 0 && str_comp_nocase(aFilename+str_length(aFilename)-4, ".clc") != 0)
			str_append(aFilename, ".lua", sizeof(aFilename)); // assume plain lua files as default
	}

	bool ret = pLF->LoadFile(aFilename, true);

	if(g_Config.m_Debug)
		dbg_msg("Lua/debug", "script '%s' %s '%s'", pLF->GetFilename()+4, ret ? "successfully exec()uted" : "failed to exec()ute", aFilename);

	// return some stuff to the script
	lua_pushboolean(L, (int)ret); // success?
	return 1;
}

/* lua call: Listdir(<string> foldername, <string/function> callback) */
int CLuaBinding::LuaListdir(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	int nargs = lua_gettop(L);
	if(nargs != 2)
		return luaL_error(L, "Listdir expects 2 arguments");

	argcheck(lua_isstring(L, 1), 1, "string"); // path
	argcheck(lua_isstring(L, 2) || lua_isfunction(L, 2), 2, "string (function name) or function"); // callback function

	// convert the function name into the actual function
	if(lua_isstring(L, 2))
	{
		lua_getglobal(L, lua_tostring(L, 2)); // check if the given callback function actually exists / retrieve the function
		argcheck(lua_isfunction(L, -1), 2, "function name (does the given function exist?)");
	}

	char aSandboxedPath[512];
	str_copyb(aSandboxedPath, lua_tostring(L, 1)); // arg1
	const char *pDir = SandboxPath(aSandboxedPath, sizeof(aSandboxedPath), pLF);
	lua_Number ret = (lua_Number)fs_listdir_verbose(pDir, LuaListdirCallback, IStorageTW::TYPE_ALL, L);
	lua_pushnumber(L, ret);
	return 1;
}

int CLuaBinding::LuaKillScript(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	pLF->Unload();
	return 0;
}

int CLuaBinding::LuaEnterFullscreen(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	CLua::m_pCGameClient->m_pMenus->LuaRequestFullscreen(pLF);
	return 0;
}

int CLuaBinding::LuaExitFullscreen(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	if(pLF != CLua::m_pClient->Lua()->GetFullscreenedScript())
		return luaL_error(L, "This script is not currently in fullscreen mode");

	CLua::m_pClient->Lua()->ExitFullscreen();
	return 0;
}

int CLuaBinding::LuaScriptPath(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	lua_pushstring(L, pLF->GetFilename());
	return 1;
}

int CLuaBinding::LuaStrIsNetAddr(lua_State *L)
{
	int nargs = lua_gettop(L);
	if(nargs != 1)
		return luaL_error(L, "StrIsNetAddr expects 1 argument");

	argcheck(lua_isstring(L, 1), 1, "string");

	NETADDR temp;
	int ret = net_addr_from_str(&temp, lua_tostring(L, 1)); // arg1
	lua_pop(L, 1), // pop arg1

			lua_pushboolean(L, ret == 0);
	return 1;
}


int CLuaBinding::LuaPrintOverride(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	int nargs = lua_gettop(L);
	if(nargs < 1)
		return luaL_error(L, "print expects 1 argument or more");

	char aSys[128];
	str_format(aSys, sizeof(aSys), "LUA|%s", pLF->GetFilename());

	// construct the message from all arguments
	char aMsg[512] = {0};
	for(int i = 1; i <= nargs; i++)
	{
		argcheck(lua_isstring(L, i) || lua_isnumber(L, i), i, "string or number");
		str_append(aMsg, lua_tostring(L, i), sizeof(aMsg));
		str_append(aMsg, "\t", sizeof(aMsg));
	}
	aMsg[str_length(aMsg)-1] = '\0'; // remove the last tab character

	// pop all to clean up the stack
	lua_pop(L, nargs);

	CGameClient *pGameClient = (CGameClient *)CLua::GameClient();
	if(pGameClient->m_pGameConsole->m_pStatLuaConsole->m_LuaHandler.m_pDebugChild == L)
		pGameClient->m_pGameConsole->m_pStatLuaConsole->PrintLine(aMsg);

	dbg_msg(aSys, "%s", aMsg);

	return 0;
}

int CLuaBinding::LuaThrow(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	int nargs = lua_gettop(L);
	if(nargs != 1)
		return luaL_error(L, "throw expects exactly 1 argument");

	argcheck(lua_isstring(L, nargs) || lua_isnumber(L, nargs), nargs, "string or number");

	// receive the current line
	lua_Debug ar;
	lua_getstack(L, 1, &ar);
	lua_getinfo(L, "nSl", &ar);
	int Line = ar.currentline;

	// add the exception
	char aMsg[512];
	str_formatb(aMsg, ":%i: Custom Exception: %s", Line, lua_tostring(L, nargs));
	int result = CLua::m_pClient->Lua()->HandleException(aMsg, pLF);

	// pop argument
	lua_pop(L, nargs);

	// push result
	lua_pushinteger(L, (lua_Integer)result);

	return 1;
}

// external info
int CLuaBinding::LuaGetPlayerScore(int ClientID)
{
	dbg_assert(ClientID >= 0 && ClientID < MAX_CLIENTS, "invalid ClientID");
	CGameClient *pGameClient = (CGameClient *)CLua::GameClient();

	const CNetObj_PlayerInfo *pInfo = pGameClient->m_Snap.m_paPlayerInfos[ClientID];
	if(pInfo)
	{
		return pInfo->m_Score;
	}

	return -1;
}

// io stuff
int CLuaBinding::LuaIO_Open(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	int nargs = lua_gettop(L);
	if(nargs < 1 || nargs > 3)
		return luaL_error(L, "io.open expects between 1 to 3 arguments");

	argcheck(lua_isstring(L, 1), 1, "string"); // path
	if(nargs >= 2)
		argcheck(lua_isstring(L, 2), 2, "string"); // mode
	if(nargs == 3)
		argcheck(lua_isstring(L, 3) && str_comp_nocase(lua_tostring(L, 3), "shared") == 0, 3, "literally \"shared\""); // shared flag

	const char *pFilename = lua_tostring(L, 1);
	const char *pOpenMode = luaL_optstring(L, 2, "r");
	bool Shared = nargs == 3;
	if(str_comp_nocase(pOpenMode, "shared") == 0)
	{
		Shared = true;
		pOpenMode = "r";
	}

	char aFilename[512];
	str_copyb(aFilename, pFilename);
	if(!Shared)
		pFilename = SandboxPath(aFilename, sizeof(aFilename), pLF);
	else
		pFilename = CLua::m_pCGameClient->Storage()->SandboxPath(aFilename, sizeof(aFilename), "lua_sandbox/_shared");


	char aFullPath[512];
	CLua::m_pCGameClient->Storage()->GetCompletePath(IStorageTW::TYPE_SAVE, aFilename, aFullPath, sizeof(aFullPath));
	if(fs_makedir_rec_for(aFullPath) != 0)
		return luaL_error(L, "Failed to create path for file '%s'", aFullPath);

	// now we make a call to the builtin function 'io.open'
	lua_pop(L, nargs); // pop our args

	// receive the original io.open that we have saved in the registry
	lua_getregistry(L);
	lua_getfield(L, -1, LUA_REGINDEX_IO_OPEN);

	// push the arguments
	lua_pushstring(L, aFullPath); // he needs the full path
	lua_pushstring(L, pOpenMode);

	// fire it off
	// this pops 3 things (function + args) and pushes 1 (resulting file handle)
	if(lua_pcall(L, 2, 1, 0) != 0)
	{
		const char *pErrorMsg = lua_isstring(L, -1) ? lua_tostring(L, -1) : "unknown";
		return luaL_error(L, "internal error: %s", pErrorMsg);
	}

	// push an additional argument for the scripter
	lua_pushstring(L, aFilename);
	return 2; // we'll leave cleaning the stack up to lua
}

void CLuaBinding::LuaRenderTexture(int ID, float x, float y, float w, float h, float rot) // depreciated
{
	CGameClient *pGameClient = (CGameClient *)CLua::GameClient();
	IGraphics *pGraphics = pGameClient->Kernel()->RequestInterface<IGraphics>();

	//pGraphics->MapScreen(0, 0, pGameClient->UI()->Screen()->w, pGameClient->UI()->Screen()->h);
	pGraphics->TextureSet(ID);
	pGraphics->QuadsBegin();
	IGraphics::CQuadItem Item;
	Item.m_X = x; Item.m_Y = y;
	Item.m_Width = w; Item.m_Height = h;
	pGraphics->QuadsSetRotation(rot);
	pGraphics->QuadsDraw(&Item, 1);
	pGraphics->QuadsEnd();

	//float Width = 400*3.0f*pGraphics->ScreenAspect();
	//float Height = 400*3.0f;
	//pGraphics->MapScreen(0, 0, Width, Height);
}

void CLuaBinding::LuaRenderQuadRaw(int x, int y, int w, int h)
{
	CGameClient *pGameClient = (CGameClient *)CLua::GameClient();
	IGraphics *pGraphics = pGameClient->Kernel()->RequestInterface<IGraphics>();

	IGraphics::CQuadItem Item;
	Item.m_X = x; Item.m_Y = y;
	Item.m_Width = w; Item.m_Height = h;
	pGraphics->QuadsDraw(&Item, 1);
}

// irc namespace
int CLuaBinding::LuaGetIrcUserlist(lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	if(!pLF)
		return luaL_error(L, "FATAL: got no lua file handler for this script?!");

	int nargs = lua_gettop(L);
	if(nargs == 0)
		return luaL_error(L, "Game.IRC:GetUserlist expects at least one argument");

	for(int i = 1; i <= nargs; i++)
	{
		argcheck(lua_isstring(L, i), i, "string");
		const char *pChanName = lua_tostring(L, i);

		CIRCCom *pCom = CLua::m_pCGameClient->m_pIRC->GetCom(std::string(pChanName));
		if(pCom)
		{
			if(pCom->GetType() == CIRCCom::TYPE_CHANNEL)
			{
				CComChan *pChan = static_cast<CComChan*>(pCom);
				const sorted_array<CComChan::CUser>& Userlist = pChan->m_Users;
				lua_createtable(L, 0, Userlist.size());
				int TableIndex = lua_gettop(L);
				for(int u = 0; u < Userlist.size(); u++)
				{
					lua_pushinteger(L, u+1); // lua is 1-based
					lua_pushstring(L, Userlist[u].c_str());
					lua_settable(L, TableIndex);
				}
			}
			else
				lua_pushstring(L, pChanName);
		}
		else
			lua_pushnil(L);
	}

	return nargs;
}

const char *CLuaBinding::SandboxPath(char *pInOutBuffer, unsigned BufferSize, lua_State *L)
{
	CLuaFile *pLF = GetLuaFile(L);
	dbg_assert_strict(pLF != NULL, "FATAL: got no lua file handler for this script?!");
	return SandboxPath(pInOutBuffer, BufferSize, pLF);
}

const char *CLuaBinding::SandboxPath(char *pInOutBuffer, unsigned BufferSize, CLuaFile *pLF)
{
	const char *pSubdir = "_shared";
	if(pLF)
		pSubdir = pLF->GetFilename();

	char aFullPath[512];
	str_formatb(aFullPath, "lua_sandbox/%s/%s", pSubdir, CLua::m_pCGameClient->Storage()->SandboxPath(pInOutBuffer, BufferSize));
	str_copy(pInOutBuffer, aFullPath, BufferSize);

	return pInOutBuffer;

}

/**
 * opens all given libraries
 */
void luaX_openlibs(lua_State *L, const luaL_Reg *lualibs)
{
	/* (following code copied from https://www.lua.org/source/5.1/linit.c.html)
	 * This is the only correct way to load standard libs!
	 * For everything non-standard (custom) use luaL_register.
	 */
	const luaL_Reg *lib = lualibs;
	for (; lib->func; lib++) {
		lua_pushcfunction(L, lib->func);
		lua_pushstring(L, lib->name);
		lua_call(L, 1, 0);
	}
}

/**
 * opens a single library
 */
void luaX_openlib(lua_State *L, const char *name, lua_CFunction func)
{
	const luaL_Reg single_lib[] = {
			{name, func},
			{NULL, NULL}
	};
	luaX_openlibs(L, single_lib);
}

/**
 * Publishes the module with given name in the global scope under the same name
 */
void luaX_register_module(lua_State *L, const char *name)
{
	/* adapted from https://www.lua.org/source/5.1/loadlib.c.html function ll_require */
	lua_getfield(L, LUA_REGISTRYINDEX, "_LOADED");
	lua_getfield(L, -1, name);
	if(!dbg_assert_strict(lua_toboolean(L, -1), "trying to register a lib that has not been opened yet!")) {  /* is it there? */
		// got whatever 'require' would return on top of the stack now
		lua_setglobal(L, name);
	}
}

void luaXopen_io(lua_State *L) { luaX_openlib(L, LUA_IOLIBNAME, luaopen_io); }

void luaXopen_os(lua_State *L) { luaX_openlib(L, LUA_OSLIBNAME, luaopen_os); }

void luaXopen_package(lua_State *L) { luaX_openlib(L, LUA_LOADLIBNAME, luaopen_package); }

void luaXopen_debug(lua_State *L) { luaX_openlib(L, LUA_DBLIBNAME, luaopen_debug); }

void luaXopen_ffi(lua_State *L) { luaX_openlib(L, LUA_FFILIBNAME, luaopen_ffi); luaX_register_module(L, "ffi"); }
