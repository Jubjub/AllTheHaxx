--_lua.GetLuaFile():SetScriptTitle("AllThehaxx Awesome Lua API testing script 2")

-- load much stuff here to show it's working and to see how long it takes
tex1 = _graphics.LoadTexture("arrow.png", -1, -1, 1)
tex2 = _graphics.LoadTexture("blob.png", -1, -1, 1)
tex3 = _graphics.LoadTexture("browse_icons.png", -1, -1, 1)
tex4 = _graphics.LoadTexture("console_bar.png", -1, -1, 1)
tex5 = _graphics.LoadTexture("debug_font.png", -1, -1, 1)
tex6 = _graphics.LoadTexture("demo_buttons.png", -1, -1, 1)
tex7 = _graphics.LoadTexture("demo_buttons2.png", -1, -1, 1)
tex8 = _graphics.LoadTexture("emoticons.png", -1, -1, 1)
tex9 = _graphics.LoadTexture("file_icons.png", -1, -1, 1)
texA = _graphics.LoadTexture("game.png", -1, -1, 1)
texB = _graphics.LoadTexture("gui_buttons.png", -1, -1, 1)
texC = _graphics.LoadTexture("gui_cursor.png", -1, -1, 1)
texD = _graphics.LoadTexture("gui_icons.png", -1, -1, 1)
texE = _graphics.LoadTexture("gui_logo.png", -1, -1, 1)
texF = _graphics.LoadTexture("menu.png", -1, -1, 1)
tex0 = _graphics.LoadTexture("particles.png", -1, -1, 1)

local function test(FadeVal)
	w = _graphics.GetScreenWidth()
	h = _graphics.GetScreenHeight()
	
	if FadeVal > 0.3 then FadeVal = 0.3 end
	_ui.SetUiColor(0,0,0,FadeVal)
	_ui.DrawUiRect(0, 0, w, h, 0, 0)
	
	-- draw some fancy textures here, using tex0-texF :D
	-- ...was to lazy for that right now.
end

RegisterEvent("OnRenderScoreboard", test)
