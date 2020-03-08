/*
 * Copyright (c) 2005 Alexander Gottwald
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 *
 * Authors:	Alexander Gottwald, Colin Harrison
 */
#include "config.h"
#include "window/util.h"
#include "resources/resources.h"
#include <msxml2.h>
#include <stdexcept>

const wchar_t *wsProgs[PROG_NUMBER]= {L"Program1", L"Program2", L"Program3", L"Program4",
	L"Program5", L"Program6", L"Program7", L"Program8", L"Program9", L"Program10"};
#define SCHEMA_IDS 3
#define LAST_SCHEMA SCHEMA_IDS - 1
const CLSID CLSID_DOMDocumentVer[SCHEMA_IDS] =
       {0x88d96a05,0xf192,0x11d4,0xa6,0x5f,0x00,0x40,0x96,0x32,0x51,0xe5,	//60
	0x88d969c0,0xf192,0x11d4,0xa6,0x5f,0x00,0x40,0x96,0x32,0x51,0xe5,	//40
	0xf5078f32,0xc551,0x11d3,0x89,0xb9,0x00,0x00,0xf8,0x1f,0xe2,0x21};	//30
#define CACHE_IDS 2
#define LAST_CACHE CACHE_IDS - 1
const CLSID CLSID_XMLSchemaCacheVer[CACHE_IDS] =
       {0x88d96a07,0xf192,0x11d4,0xa6,0x5f,0x00,0x40,0x96,0x32,0x51,0xe5,	//60
	0x88d969c2,0xf192,0x11d4,0xa6,0x5f,0x00,0x40,0x96,0x32,0x51,0xe5};	//40
const IID IID_IXMLDOMDocument2 = {0x2933BF95,0x7B36,0x11d2,0xB2,0x0E,0x00,0xC0,0x4F,0x98,0x3E,0x60};
const IID IID_IXMLDOMSchemaCollection2 = {0x50EA08B0,0xDD1B,0x4664,0x9A,0x50,0xC2,0xF4,0x0F,0x4B,0xD7,0x9A};

#define HRCALL(x, msg) if (FAILED(x)) { throw std::runtime_error("OLE Error:" msg " failed"); };

VARIANT VariantString(LPCWSTR wstr)
{
   VARIANT var;
   VariantInit(&var);
   V_BSTR(&var) = SysAllocString(wstr);
   V_VT(&var) = VT_BSTR;
   return var;
}

IXMLDOMDocument2 *CreateDocument(bool validate)
{
    IXMLDOMDocument2 *doc = NULL;
    IXMLDOMSchemaCollection *schema = NULL;
    bool oldschema = false;
    wchar_t szMessage[MAX_MESSAGE];
    wchar_t szCaption[MAX_CAPTION];
    
    CoInitialize(NULL);

    for (int i=0; i < SCHEMA_IDS; i++)
    { 
	if (i == LAST_SCHEMA) oldschema = true;
	if (validate && i == LAST_SCHEMA) //Can't validate as no new schema found
	{
		LoadStringW(GetModuleHandle(NULL), IDS_MSXML4_MESS, szMessage, sizeof(szMessage)/sizeof(wchar_t));
		LoadStringW(GetModuleHandle(NULL), IDS_MSXML4_CAP, szCaption, sizeof(szCaption)/sizeof(wchar_t));
		MessageBoxW(NULL, szMessage, szCaption, MB_OK | MB_ICONINFORMATION);
	}
	if (CoCreateInstance(CLSID_DOMDocumentVer[i], NULL, CLSCTX_INPROC_SERVER,
				IID_IXMLDOMDocument2, (void**)&doc) == S_OK) break;
	if (i == LAST_SCHEMA) //Can't load the config file
	{
		LoadStringW(GetModuleHandle(NULL), IDS_MSXML34_MESS, szMessage, sizeof(szMessage)/sizeof(wchar_t));
		LoadStringW(GetModuleHandle(NULL), IDS_CANNOT_OPEN, szCaption, sizeof(szCaption)/sizeof(wchar_t));
		MessageBoxW(NULL, szMessage, szCaption, MB_OK | MB_ICONEXCLAMATION);
		return NULL;
	}
    }

    if (!oldschema)
    {
	for (int i=0; i < CACHE_IDS; i++)
	{
		if (CoCreateInstance(CLSID_XMLSchemaCacheVer[i], NULL, CLSCTX_SERVER,
					IID_IXMLDOMSchemaCollection2, (void**)&schema) == S_OK) break;
		if (i == LAST_CACHE) //Can't use a schema cache
		{
			LoadStringW(GetModuleHandle(NULL), IDS_MSXML4_CACHE, szMessage, sizeof(szMessage)/sizeof(wchar_t));
			LoadStringW(GetModuleHandle(NULL), IDS_CANNOT_OPEN, szCaption, sizeof(szCaption)/sizeof(wchar_t));
			MessageBoxW(NULL, szMessage, szCaption, MB_OK | MB_ICONEXCLAMATION);
			return NULL;
		}
	}

	if (schema->add(SysAllocString(L"http://www.straightrunning.com/XmingNotes"),
					VariantString(L"XLaunch.xsd")) != S_OK)
	{
		LoadStringW(GetModuleHandle(NULL), IDS_SCHEMA_MESS, szMessage, sizeof(szMessage)/sizeof(wchar_t));
		LoadStringW(GetModuleHandle(NULL), IDS_SCHEMA_CAP, szCaption, sizeof(szCaption)/sizeof(wchar_t));
		MessageBoxW(NULL, szMessage, szCaption, MB_OK | MB_ICONINFORMATION);
	}
    }

    try {
	if (!oldschema)
	{
		VARIANT v_Value;
		v_Value.vt = VT_DISPATCH;
		v_Value.pdispVal = schema;
		HRCALL(doc->putref_schemas(v_Value), "putref_schemas");
		VariantClear(&v_Value);
	}

	HRCALL(doc->put_async(VARIANT_FALSE), "put_async");
	if (validate)
	{
		HRCALL(doc->put_validateOnParse(VARIANT_TRUE), "put_validateOnParse");
	}
	else
	{
		HRCALL(doc->put_validateOnParse(VARIANT_FALSE), "put_validateOnParse");
	}
	HRCALL(doc->put_resolveExternals(VARIANT_FALSE), "put_resolveExternals");

	IXMLDOMProcessingInstruction *pi = NULL;
	IXMLDOMElement *root = NULL;
     	BSTR xml = SysAllocString(L"xml");
	BSTR ver = SysAllocString(L"version='1.0'");
	HRCALL(doc->createProcessingInstruction(xml,ver, &pi), 
		"createProcessingInstruction");
	HRCALL(doc->appendChild(pi, NULL),
		"appendChild");
	pi->Release();
	SysFreeString(xml);
	SysFreeString(ver);

	BSTR elemname = SysAllocString(L"XLaunch");
	HRCALL(doc->createElement(elemname, &root), "createElement");
	HRCALL(doc->appendChild(root, NULL), "appendChild");
	SysFreeString(elemname);
    } catch (...)
    {
	if (!oldschema)
		schema->Release();
	doc->Release();
	throw;
    }
    return doc;
}

void setAttribute(IXMLDOMElement *elem, LPCWSTR name, LPCWSTR value)
{
    if (!wcscmp(value,L"")) return;
    BSTR bstr = SysAllocString(name);
    VARIANT var = VariantString(value);
    HRCALL(elem->setAttribute(bstr, var), "setAttribute");
    VariantClear(&var);
    SysFreeString(bstr);
}

void CConfig::Save(LPCWSTR filename, bool validate)
{
    IXMLDOMDocument2 *doc = CreateDocument(validate);
    if (!doc) return;
    IXMLDOMElement *root = NULL;

    HRCALL(doc->get_documentElement(&root), "get_documentElement");

    setAttribute(root, L"xmlns", L"http://www.straightrunning.com/XmingNotes");
    setAttribute(root, L"xmlns:xsi", L"http://www.w3.org/2001/XMLSchema-instance");
    setAttribute(root, L"xsi:schemaLocation", L"http://www.straightrunning.com/XmingNotes XLaunch.xsd");

    switch (window)
    {
	case MultiWindow:
	    setAttribute(root, L"WindowMode", L"MultiWindow");
	    break;
	case Fullscreen:
	    setAttribute(root, L"WindowMode", L"Fullscreen");
	    break;
	default:
	case Windowed:
	    setAttribute(root, L"WindowMode", L"Windowed");
	    break;
	case Nodecoration:
	    setAttribute(root, L"WindowMode", L"Nodecoration");
	    break;
    }
    switch (client)
    {
	default:
	case NoClient:
	    setAttribute(root, L"ClientMode", L"NoClient");
	    break;
	case StartProgram:
	    setAttribute(root, L"ClientMode", L"StartProgram");
	    setAttribute(root, L"Program", program.c_str());
	    for (int i = 0; i < PROG_NUMBER; i++) setAttribute(root, wsProgs[i], progs[i].c_str());
	    switch (clientstart)
	    {
		default:
		case NoXClient:
		    break;
		case Local:
		    setAttribute(root, L"ClientStart", L"Local");
		    break;
		case PuTTY:
		    setAttribute(root, L"ClientStart", L"PuTTY");
		    if (password_save) setAttribute(root, L"PW", password.c_str());
		    if (password_start && !password_save) setAttribute(root, L"PasswordStart", L"true");
		    setAttribute(root, L"PathToProtocol", protocol_path.c_str());
		    if (compress)
			setAttribute(root, L"Compress", L"true");
		    setAttribute(root, L"RemoteHost", host.c_str());
		    setAttribute(root, L"RemoteUser", user.c_str());
		    setAttribute(root, L"ExtraSSH", extra_ssh.c_str());
		    break;
		case OpenSSH:
		    setAttribute(root, L"ClientStart", L"OpenSSH");
		    setAttribute(root, L"PathToProtocol", protocol_path.c_str());
		    if (compress)
			setAttribute(root, L"Compress", L"true");
		    setAttribute(root, L"RemoteHost", host.c_str());
		    setAttribute(root, L"RemoteUser", user.c_str());
		    setAttribute(root, L"ExtraSSH", extra_ssh.c_str());
		    break;
	    }
	    break;
	case XDMCP:
	    setAttribute(root, L"ClientMode", L"XDMCP");
	    if (broadcast)
		setAttribute(root, L"XDMCPBroadcast", L"true");
	    else
	    {
		if (indirect)
			setAttribute(root, L"XDMCPIndirect", L"true");
		setAttribute(root, L"XDMCPHost", xdmcp_host.c_str());
	    }
	    break;
    }
    setAttribute(root, L"Display", display.c_str());
    setAttribute(root, L"Clipboard", clipboard?L"true":L"false");
    if (no_access_control)
	setAttribute(root, L"NoAccessControl", L"true");
    setAttribute(root, L"FontServer", font_server.c_str());
    setAttribute(root, L"ExtraParams", extra_params.c_str());

    VARIANT var = VariantString(filename);
    HRCALL(doc->save(var), "save");
    VariantClear(&var);


    root->Release();
    doc->Release();
}

BOOL getAttribute(IXMLDOMElement *elem, LPCWSTR name, std::wstring &ret)
{
    VARIANT var;
    HRCALL(elem->getAttribute((OLECHAR*)name, &var), "getAttribute"); 
    if (V_VT(&var) != VT_NULL && V_VT(&var) == VT_BSTR)
    {
	wchar_t *wstr = V_BSTR(&var);
	ret = wstr;
	delete [] wstr;
	return true;
    }
    return false;
}

BOOL getAttributeBool(IXMLDOMElement *elem, LPCWSTR name, bool &ret)
{
    std::wstring wstr;
    if (getAttribute(elem, name, wstr))
    {
	if (wstr == L"True" || wstr == L"true")
	    ret = true;
	else
	    ret = false;
	return true;
    }
    return false;
}


void CConfig::Load(LPCWSTR filename, bool validate)
{
    IXMLDOMDocument2 *doc = CreateDocument(validate);
    if (!doc) return;
    IXMLDOMElement *root = NULL;
    HRESULT hr;

    VARIANT var = VariantString(filename);
    VARIANT_BOOL status;
    HRCALL(doc->load(var, &status), "load");
    VariantClear(&var);

    if (status == VARIANT_FALSE)
    {
	IXMLDOMParseError * p_ParseError = NULL;
	wchar_t szCaption[MAX_CAPTION];
	LoadStringW(GetModuleHandle(NULL), IDS_INVALID_CAP, szCaption, sizeof(szCaption)/sizeof(wchar_t));
	HRCALL(doc->get_parseError(&p_ParseError), "get_parseError");
	if (p_ParseError !=0)
	{
	   long l_Line;
	   long l_Pos;
	   wchar_t buffer[512], szMessage[MAX_MESSAGE];
	   BSTR bstr_Value = 0;
	   HRCALL(p_ParseError->get_reason(&bstr_Value), "get_reason");
	   p_ParseError->get_line(&l_Line);
	   p_ParseError->get_linepos(&l_Pos);
	   p_ParseError->Release();
	   LoadStringW(GetModuleHandle(NULL), IDS_INVALID_MESS, szMessage, sizeof(szMessage)/sizeof(wchar_t));
	   swprintf(buffer, szMessage, filename, l_Line, l_Pos, (LPCWSTR)bstr_Value);
	   MessageBoxW(NULL, buffer, szCaption, MB_OK | MB_ICONEXCLAMATION);
	   SysFreeString(bstr_Value);
	}
	else
	{
	   MessageBoxW(NULL, filename, szCaption, MB_OK | MB_ICONEXCLAMATION);
	}
	doc->Release();
	return;
    }

//  Trap if namespace headers missing
    if (validate)
    {
	IXMLDOMParseError * p_ParseError = NULL;
	hr = doc->validate(&p_ParseError);
	if (hr != S_OK)
	{
	   wchar_t buffer[512], szMessage[MAX_MESSAGE], szCaption[MAX_CAPTION];
	   BSTR bstr_Value = 0;
	   HRCALL(p_ParseError->get_reason(&bstr_Value), "get_reason");
	   LoadStringW(GetModuleHandle(NULL), IDS_TRY_SAVE_MESS, szMessage, sizeof(szMessage)/sizeof(wchar_t));
	   LoadStringW(GetModuleHandle(NULL), IDS_TRY_SAVE_CAP, szCaption, sizeof(szCaption)/sizeof(wchar_t));
	   swprintf(buffer, szMessage, filename, (LPCWSTR)bstr_Value);
	   MessageBoxW(NULL, buffer, szCaption, MB_OK | MB_ICONINFORMATION);
	   SysFreeString(bstr_Value);
	}
	p_ParseError->Release();
    }

    HRCALL(doc->get_documentElement(&root), "get_documentElement");

    std::wstring windowMode;
    std::wstring clientMode;
    std::wstring clientStart;

    if (getAttribute(root, L"WindowMode", windowMode))
    {
	if (windowMode == L"MultiWindow")
	    window = MultiWindow;
	else if (windowMode == L"Fullscreen")
	    window = Fullscreen;
	else if (windowMode == L"Windowed")
	    window = Windowed;
	else if (windowMode == L"Nodecoration")
	    window = Nodecoration;
    }
    if (getAttribute(root, L"ClientMode", clientMode))
    {
	if (clientMode == L"NoClient")
	    client = NoClient;
	else if (clientMode == L"StartProgram")
	    client = StartProgram;
	else if (clientMode == L"XDMCP")
	    client = XDMCP;
    }
    if (getAttribute(root, L"ClientStart", clientStart))
    {
	if (clientStart == L"Local")
	    clientstart = Local;
	else if (clientStart == L"PuTTY")
	    clientstart = PuTTY;
	else if (clientStart == L"OpenSSH")
	    clientstart = OpenSSH;
    }
    
    getAttribute(root, L"Display", display);
    getAttribute(root, L"Program", program);
    for (int i = 0; i < PROG_NUMBER; i++) getAttribute(root, wsProgs[i], progs[i]);
    getAttribute(root, L"PathToProtocol", protocol_path);
    getAttributeBool(root, L"Compress", compress);
    getAttribute(root, L"RemoteHost", host);
    getAttribute(root, L"RemoteUser", user);
    getAttribute(root, L"PW", password);
    getAttributeBool(root, L"PasswordStart", password_start);
    getAttribute(root, L"XDMCPHost", xdmcp_host);
    getAttributeBool(root, L"XDMCPBroadcast", broadcast);
    getAttributeBool(root, L"XDMCPIndirect", indirect);
    getAttributeBool(root, L"Clipboard", clipboard);
    getAttributeBool(root, L"NoAccessControl", no_access_control);
    getAttribute(root, L"FontServer", font_server);
    getAttribute(root, L"ExtraParams", extra_params);
    getAttribute(root, L"ExtraSSH", extra_ssh);
    

    doc->Release();
}

