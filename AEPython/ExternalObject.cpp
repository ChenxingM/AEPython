#include "../Libs/ExtendScript-Toolkit/SoSharedLibDefs.h"
#include "../Libs/ExtendScript-Toolkit/SoCClient.h"

#include "PythonInstance.h"

#include <cstring>
#include <cstdlib>

#ifdef _WIN32
	#define DllExport extern "C" __declspec( dllexport )
#else
	#define DllExport extern "C" __attribute__((visibility("default")))
#endif

SoServerInterface* gpServer = nullptr;

static char* stringToCharP(const char* src)
{
	const auto length = std::strlen(src) + 1;
	char* dst = static_cast<char*>(std::malloc(length));
	if (dst) std::memcpy(dst, src, length);

	return dst;
}

// Python.exec("Python code string");
DllExport long _exec(TaggedData* argv, long argc, TaggedData* retval)
{
	if (argc != 2 || argv[0].type != kTypeString || argv[1].type != kTypeString)
	{
		return kESErrBadArgumentList;
	}

	bool success = AEPython_exec(argv[0].data.string, argv[1].data.string);

	retval->type = kTypeUndefined;

	return success ? kESErrOK : kESErrEval;
}

// Python.eval("Python code string");
DllExport long _eval(TaggedData* argv, long argc, TaggedData* retval)
{
	if (argc != 2 || argv[0].type != kTypeString || argv[1].type != kTypeString)
	{
		return kESErrBadArgumentList;
	}

	auto code = argv[0].data.string;
	auto stack = argv[1].data.string;
	char* ret = AEPython_eval(code, stack);

	if (ret == nullptr)
	{
		return kESErrEval;
	}

	retval->data.string = stringToCharP(ret);
	AEPython_free(ret);
	if (retval->data.string == nullptr)
	{
		return kESErrEval;
	}
	retval->type = kTypeScript;
	return kESErrOK;
}

DllExport void ESFreeMem(void* p)
{
	std::free(p);
}

DllExport long ESGetVersion()
{
	return 2;
}

DllExport char* ESInitialize(const TaggedData** argv, long argc)
{
	return (char*)"_exec_ss,_eval_ss";
}

DllExport void ESTerminate()
{
}

DllExport void* ESMallocMem(size_t nBytes)
{
	return malloc(nBytes);
}

ESerror_t PyObjectBase_initialize(SoHObject hObject, int argc, TaggedData* argv)
{
	if (argc == 1 && argv[0].type == kTypeInteger)
	{
		auto* id = new long(argv[0].data.intval);
		gpServer->setClientData(hObject, id);
		return kESErrOK;
	}
	else
	{
		gpServer->setClientData(hObject, nullptr);
		return kESErrBadArgumentList;
	}
}

ESerror_t PyObjectBase_finalize(SoHObject hObject)
{
	long* id;
	gpServer->getClientData(hObject, (void**)&id);

	if (id != nullptr)
	{
		AEPython_del_py_object(*id);
		delete id;
	}

	return kESErrOK;
}

SoObjectInterface objectInterface =
{
	PyObjectBase_initialize,
	nullptr, // pull
	nullptr, // get
	nullptr, // call
	nullptr, // valueOf
	nullptr, // toString
	PyObjectBase_finalize
};

DllExport int  ESClientInterface(SoCClient_e kReason, SoServerInterface* pServer, SoHServer hServer)
{
	if (kReason == kSoCClient_init)
	{
		gpServer = pServer;
		gpServer->addClass(hServer, "AEPython_PyObjectBase", &objectInterface);
	}

	return 0;
}
