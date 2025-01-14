/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef WKBundlePageFormClient_h
#define WKBundlePageFormClient_h

#include <WebKit/WKBase.h>
#include <WebKit/WKBundlePageEditorClient.h>
#include <webkitdom/webkitdom.h>

typedef void (*WKBundlePageTextFieldDidBeginEditingCallback)(WKBundlePageRef page, WebKitDOMHTMLInputElement * inputElement, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageTextFieldDidEndEditingCallback)(WKBundlePageRef page, WebKitDOMHTMLInputElement * inputElement, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageTextDidChangeInTextFieldCallback)(WKBundlePageRef page, WebKitDOMHTMLInputElement * inputElement, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageTextDidChangeInTextAreaCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlTextAreaElementHandle, WKBundleFrameRef frame, const void* clientInfo);
typedef bool (*WKBundlePageShouldPerformActionInTextFieldCallback)(WKBundlePageRef page, WKBundleNodeHandleRef htmlInputElementHandle, WKInputFieldActionType actionType, WKBundleFrameRef frame, const void* clientInfo);
typedef void (*WKBundlePageWillSubmitFormCallback)(WKBundlePageRef page, WebKitDOMHTMLFormElement * form, WKBundleFrameRef frame, WKBundleFrameRef sourceFrame, WKDictionaryRef values, WKTypeRef* userData, const void* clientInfo);
typedef void (*WKBundlePageWillSendSubmitEventCallback)(WKBundlePageRef page, WebKitDOMHTMLFormElement * form, WKBundleFrameRef frame, WKBundleFrameRef sourceFrame, WKDictionaryRef values, const void* clientInfo);
typedef void (*WKBundlePageWillSendUsernamePasswordStoreEventCallback)();//lxx, 20150816
typedef void (*WKBundlePageDidFocusTextFieldCallback)(WKBundlePageRef page, WebKitDOMHTMLInputElement * inputElement, WKBundleFrameRef frame, const void* clientInfo);
typedef bool (*WKBundlePageShouldNotifyOnFormChangesCallback)(WKBundlePageRef page, const void* clientInfo);
typedef void (*WKBundlePageDidAssociateFormControlsCallback)(WKBundlePageRef page, WKArrayRef elementHandles, const void* clientInfo);

typedef struct WKBundlePageFormClientBase {
    int                                                                 version;
    const void *                                                        clientInfo;
} WKBundlePageFormClientBase;

typedef struct WKBundlePageFormClientV0 {
    WKBundlePageFormClientBase                                          base;

    // Version 0.
    WKBundlePageTextFieldDidBeginEditingCallback                        textFieldDidBeginEditing;
    WKBundlePageTextFieldDidEndEditingCallback                          textFieldDidEndEditing;
    WKBundlePageTextDidChangeInTextFieldCallback                        textDidChangeInTextField;
    WKBundlePageTextDidChangeInTextAreaCallback                         textDidChangeInTextArea;
    WKBundlePageShouldPerformActionInTextFieldCallback                  shouldPerformActionInTextField;
    WKBundlePageWillSubmitFormCallback                                  willSubmitForm;
    WKBundlePageWillSendUsernamePasswordStoreEventCallback              willSendUsernamePasswordStoreEvent;//lxx, 20150816
} WKBundlePageFormClientV0;

typedef struct WKBundlePageFormClientV1 {
    WKBundlePageFormClientBase                                          base;

    // Version 0.
    WKBundlePageTextFieldDidBeginEditingCallback                        textFieldDidBeginEditing;
    WKBundlePageTextFieldDidEndEditingCallback                          textFieldDidEndEditing;
    WKBundlePageTextDidChangeInTextFieldCallback                        textDidChangeInTextField;
    WKBundlePageTextDidChangeInTextAreaCallback                         textDidChangeInTextArea;
    WKBundlePageShouldPerformActionInTextFieldCallback                  shouldPerformActionInTextField;
    WKBundlePageWillSubmitFormCallback                                  willSubmitForm;

    // Version 1.
    WKBundlePageWillSendSubmitEventCallback                             willSendSubmitEvent;
    WKBundlePageWillSendUsernamePasswordStoreEventCallback              willSendUsernamePasswordStoreEvent;//lxx, 20150816	
} WKBundlePageFormClientV1;

typedef struct WKBundlePageFormClientV2 {
    WKBundlePageFormClientBase                                          base;

    // Version 0.
    WKBundlePageTextFieldDidBeginEditingCallback                        textFieldDidBeginEditing;
    WKBundlePageTextFieldDidEndEditingCallback                          textFieldDidEndEditing;
    WKBundlePageTextDidChangeInTextFieldCallback                        textDidChangeInTextField;
    WKBundlePageTextDidChangeInTextAreaCallback                         textDidChangeInTextArea;
    WKBundlePageShouldPerformActionInTextFieldCallback                  shouldPerformActionInTextField;
    WKBundlePageWillSubmitFormCallback                                  willSubmitForm;
    // Version 1.
    WKBundlePageWillSendSubmitEventCallback                             willSendSubmitEvent;
    WKBundlePageWillSendUsernamePasswordStoreEventCallback              willSendUsernamePasswordStoreEvent;//lxx, 20150816
    // version 2.
    WKBundlePageDidFocusTextFieldCallback                               didFocusTextField;
    WKBundlePageShouldNotifyOnFormChangesCallback                       shouldNotifyOnFormChanges;
    WKBundlePageDidAssociateFormControlsCallback                        didAssociateFormControls;
} WKBundlePageFormClientV2;

enum { kWKBundlePageFormClientCurrentVersion WK_ENUM_DEPRECATED("Use an explicit version number instead") = 2 };
typedef struct WKBundlePageFormClient {
    int                                                                 version;
    const void *                                                        clientInfo;
    
    // Version 0.
    WKBundlePageTextFieldDidBeginEditingCallback                        textFieldDidBeginEditing;
    WKBundlePageTextFieldDidEndEditingCallback                          textFieldDidEndEditing;
    WKBundlePageTextDidChangeInTextFieldCallback                        textDidChangeInTextField;
    WKBundlePageTextDidChangeInTextAreaCallback                         textDidChangeInTextArea;
    WKBundlePageShouldPerformActionInTextFieldCallback                  shouldPerformActionInTextField;
    WKBundlePageWillSubmitFormCallback                                  willSubmitForm;
    // Version 1.
    WKBundlePageWillSendSubmitEventCallback                             willSendSubmitEvent;
    WKBundlePageWillSendUsernamePasswordStoreEventCallback              willSendUsernamePasswordStoreEvent;//lxx, 20150816
    // version 2.
    WKBundlePageDidFocusTextFieldCallback                               didFocusTextField;
    WKBundlePageShouldNotifyOnFormChangesCallback                       shouldNotifyOnFormChanges;
    WKBundlePageDidAssociateFormControlsCallback                        didAssociateFormControls;
} WKBundlePageFormClient WK_C_DEPRECATED("Use an explicit versioned struct instead");

#endif // WKBundlePageFormClient_h
