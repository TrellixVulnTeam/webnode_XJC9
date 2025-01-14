/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebCookieManager.h"

#include "ChildProcess.h"
#include "WebCookieManagerMessages.h"
#include "WebCookieManagerProxyMessages.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/CookieStorage.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformCookieJar.h>
//add by luyue 2015/6/16 start
#include <WebCore/CookieJarSoup.h>
#include <WebCore/Cookie.h>
#include <WebCore/GUniquePtrSoup.h>
#include <WebCore/URL.h>
#include <WebCore/NetworkingContext.h>
#include "SoupNetworkSession.h"
//add end
#include <wtf/MainThread.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

static WebCookieManager* sharedCookieManager;

const char* WebCookieManager::supplementName()
{
    return "WebCookieManager";
}

WebCookieManager::WebCookieManager(ChildProcess* process)
    : m_process(process)
{
    m_process->addMessageReceiver(Messages::WebCookieManager::messageReceiverName(), *this);

    ASSERT(!sharedCookieManager);
    sharedCookieManager = this;
}

void WebCookieManager::getHostnamesWithCookies(uint64_t callbackID)
{
    HashSet<String> hostnames;
    WebCore::getHostnamesWithCookies(NetworkStorageSession::defaultStorageSession(), hostnames);

    Vector<String> hostnameList;
    copyToVector(hostnames, hostnameList);

    m_process->send(Messages::WebCookieManagerProxy::DidGetHostnamesWithCookies(hostnameList, callbackID), 0);
}

void WebCookieManager::deleteCookiesForHostname(const String& hostname)
{
    WebCore::deleteCookiesForHostname(NetworkStorageSession::defaultStorageSession(), hostname);
}

void WebCookieManager::deleteAllCookies()
{
    WebCore::deleteAllCookies(NetworkStorageSession::defaultStorageSession());
}

void WebCookieManager::deleteAllCookiesModifiedSince(std::chrono::system_clock::time_point time)
{
    WebCore::deleteAllCookiesModifiedSince(NetworkStorageSession::defaultStorageSession(), time);
}

void WebCookieManager::startObservingCookieChanges()
{
    WebCore::startObservingCookieChanges(cookiesDidChange);
}

void WebCookieManager::stopObservingCookieChanges()
{
    WebCore::stopObservingCookieChanges();
}

void WebCookieManager::cookiesDidChange()
{
    sharedCookieManager->dispatchCookiesDidChange();
}

//add by luyue 2015/6/16 start
void WebCookieManager::getCookiesWithUrl(const String& base_url)
{
   SoupCookieJar* cookieJar = (NetworkStorageSession::defaultStorageSession()).soupNetworkSession().cookieJar();
   SoupURI *uri = soup_uri_new (base_url.utf8().data());
   char*cookie = soup_cookie_jar_get_cookies(cookieJar,uri,true);
   m_process->send(Messages::WebCookieManagerProxy::GetCookies(String::fromUTF8(cookie)),0);
}
//add end 

void WebCookieManager::dispatchCookiesDidChange()
{
    ASSERT(RunLoop::isMain());
    m_process->send(Messages::WebCookieManagerProxy::CookiesDidChange(), 0);
}

void WebCookieManager::setHTTPCookieAcceptPolicy(HTTPCookieAcceptPolicy policy)
{
    platformSetHTTPCookieAcceptPolicy(policy);
}

void WebCookieManager::getHTTPCookieAcceptPolicy(uint64_t callbackID)
{
    m_process->send(Messages::WebCookieManagerProxy::DidGetHTTPCookieAcceptPolicy(platformGetHTTPCookieAcceptPolicy(), callbackID), 0);
}

} // namespace WebKit
