#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"

#include <string>

class ICodeEditorHost {
public:
    virtual ~ICodeEditorHost() = default;
    virtual std::string editorSource() = 0;
    virtual bool hasEditorSource() = 0;
    virtual void setEditorSource(const std::string& source) = 0;
    virtual std::string checkCompatibility(const std::string& source) = 0;
    virtual void submitCode(const std::string& source) = 0;
    virtual void stopPattern() = 0;
    virtual std::string editorStatus() = 0;
};

Steinberg::IPlugView* createCodeEditorView(ICodeEditorHost* host,
                                          Steinberg::FUnknown* keepAlive);
