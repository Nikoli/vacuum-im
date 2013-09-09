#include "crashhandler.h"

#include <QtCore/QDir>
#include <QtCore/QProcess>
#include <QtCore/QCoreApplication>

#if defined(Q_OS_MAC)
#include <thirdparty/breakpad/client/mac/handler/exception_handler.h>
#elif defined(Q_OS_LINUX)
#include <thirdparty/breakpad/client/linux/handler/exception_handler.h>
#elif defined(Q_OS_WIN32)
#include <thirdparty/breakpad/client/windows/handler/exception_handler.h>
#endif

// CrashHandlerPrivate

class CrashHandlerPrivate
{
public:
	CrashHandlerPrivate()
	{
		pHandler = NULL;
	}

	~CrashHandlerPrivate()
	{
		delete pHandler;
	}

	void initCrashHandler(const QString& dumpPath);
	static google_breakpad::ExceptionHandler* pHandler;
	static bool bReportCrashesToSystem;
};

google_breakpad::ExceptionHandler* CrashHandlerPrivate::pHandler = NULL;
bool CrashHandlerPrivate::bReportCrashesToSystem = false;

// DumpCallback

#if defined(Q_OS_WIN32)
bool DumpCallback(const wchar_t* _dump_dir ,const wchar_t* _minidump_id, void* context, EXCEPTION_POINTERS* exinfo, MDRawAssertionInfo* assertion, bool success)
#elif defined(Q_OS_LINUX)
bool DumpCallback(const google_breakpad::MinidumpDescriptor &md, void *context, bool success)
#elif defined(Q_OS_MAC)
bool DumpCallback(const char* _dump_dir, const char* _minidump_id, void *context, bool success)
#endif
{
	Q_UNUSED(context);
#if defined(Q_OS_WIN32)
	Q_UNUSED(_dump_dir);
	Q_UNUSED(_minidump_id);
	Q_UNUSED(assertion);
	Q_UNUSED(exinfo);
#endif

/*	NO STACK USE, NO HEAP USE THERE! Creating QString, using qDebug, etc. everything is crash unfriendly. */

	return CrashHandlerPrivate::bReportCrashesToSystem ? success : true;
}

void CrashHandlerPrivate::initCrashHandler(const QString& dumpPath)
{
	if ( pHandler != NULL )
		return;

	QDir dumpDir(dumpPath);
	if (!dumpDir.exists(dumpPath))
		dumpDir.mkdir(dumpPath);

#if defined(Q_OS_WIN32)
	std::wstring pathAsStr = (const wchar_t*)dumpPath.utf16();
	pHandler = new google_breakpad::ExceptionHandler(pathAsStr, /*FilterCallback*/ 0, DumpCallback,	/*context*/	0, true);
#elif defined(Q_OS_LINUX)
	std::string pathAsStr = dumpPath.toStdString();
	google_breakpad::MinidumpDescriptor md(pathAsStr);
	pHandler = new google_breakpad::ExceptionHandler(md, /*FilterCallback*/ 0, DumpCallback, /*context*/ 0,	true, -1);
#elif defined(Q_OS_MAC)
	std::string pathAsStr = dumpPath.toStdString();
	pHandler = new google_breakpad::ExceptionHandler(pathAsStr,	/*FilterCallback*/ 0, DumpCallback,	/*context*/	0, true, NULL		);
#endif
}

// CrashHandler

CrashHandler* CrashHandler::instance()
{
	static CrashHandler globalHandler;
	return &globalHandler;
}

CrashHandler::CrashHandler()
{
	d = new CrashHandlerPrivate();
}

CrashHandler::~CrashHandler()
{
	delete d;
}

void CrashHandler::setReportCrashesToSystem(bool report)
{
	d->bReportCrashesToSystem = report;
}

bool CrashHandler::writeMinidump()
{
	bool res = d->pHandler->WriteMinidump();
//	if (res)
//		qDebug("WriteMinidump successed.");
//	else
//		qWarning("WriteMinidump failed.");
	return res;
}

void CrashHandler::init(const QString &reportPath)
{
	d->initCrashHandler(reportPath);
}
