/*============================================================================
  CMake - Cross Platform Makefile Generator
  Copyright 2000-2010 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#include "cmCustomCommandGenerator.h"

#include "cmCustomCommand.h"
#include "cmGeneratorExpression.h"
#include "cmLocalGenerator.h"
#include "cmMakefile.h"
#include "cmOutputConverter.h"

cmCustomCommandGenerator::cmCustomCommandGenerator(cmCustomCommand const& cc,
                                                   const std::string& config,
                                                   cmLocalGenerator* lg)
  : CC(cc)
  , Config(config)
  , LG(lg)
  , OldStyle(cc.GetEscapeOldStyle())
  , MakeVars(cc.GetEscapeAllowMakeVars())
  , GE(new cmGeneratorExpression(cc.GetBacktrace()))
  , DependsDone(false)
{
}

cmCustomCommandGenerator::~cmCustomCommandGenerator()
{
  delete this->GE;
}

unsigned int cmCustomCommandGenerator::GetNumberOfCommands() const
{
  return static_cast<unsigned int>(this->CC.GetCommandLines().size());
}

bool cmCustomCommandGenerator::UseCrossCompilingEmulator(unsigned int c) const
{
  std::string const& argv0 = this->CC.GetCommandLines()[c][0];
  cmGeneratorTarget* target = this->LG->FindGeneratorTargetToUse(argv0);
  if (target && target->GetType() == cmState::EXECUTABLE) {
    return target->GetProperty("CROSSCOMPILING_EMULATOR") != CM_NULLPTR;
  }
  return false;
}

std::string cmCustomCommandGenerator::GetCommand(unsigned int c) const
{
  std::string const& argv0 = this->CC.GetCommandLines()[c][0];
  cmGeneratorTarget* target = this->LG->FindGeneratorTargetToUse(argv0);
  if (target && target->GetType() == cmState::EXECUTABLE &&
      (target->IsImported() ||
       !this->LG->GetMakefile()->IsOn("CMAKE_CROSSCOMPILING"))) {
    return target->GetLocation(this->Config);
  }
  if (target && target->GetType() == cmState::EXECUTABLE) {
    const char* emulator = target->GetProperty("CROSSCOMPILING_EMULATOR");
    if (emulator) {
      return std::string(emulator);
    }
  }

  cmsys::auto_ptr<cmCompiledGeneratorExpression> cge = this->GE->Parse(argv0);
  std::string exe = cge->Evaluate(this->LG, this->Config);

  return exe;
}

std::string escapeForShellOldStyle(const std::string& str)
{
  std::string result;
#if defined(_WIN32) && !defined(__CYGWIN__)
  // if there are spaces
  std::string temp = str;
  if (temp.find(" ") != std::string::npos &&
      temp.find("\"") == std::string::npos) {
    result = "\"";
    result += str;
    result += "\"";
    return result;
  }
  return str;
#else
  for (const char* ch = str.c_str(); *ch != '\0'; ++ch) {
    if (*ch == ' ') {
      result += '\\';
    }
    result += *ch;
  }
  return result;
#endif
}

void cmCustomCommandGenerator::AppendArguments(unsigned int c,
                                               std::string& cmd) const
{
  unsigned int offset = 1;
  if (this->UseCrossCompilingEmulator(c)) {
    offset = 0;
  }
  cmCustomCommandLine const& commandLine = this->CC.GetCommandLines()[c];
  for (unsigned int j = offset; j < commandLine.size(); ++j) {
    std::string arg =
      this->GE->Parse(commandLine[j])->Evaluate(this->LG, this->Config);
    cmd += " ";
    if (this->OldStyle) {
      cmd += escapeForShellOldStyle(arg);
    } else {
      cmOutputConverter converter(this->LG->GetStateSnapshot());
      cmd += converter.EscapeForShell(arg, this->MakeVars);
    }
  }
}

const char* cmCustomCommandGenerator::GetComment() const
{
  return this->CC.GetComment();
}

std::string cmCustomCommandGenerator::GetWorkingDirectory() const
{
  return this->CC.GetWorkingDirectory();
}

std::vector<std::string> const& cmCustomCommandGenerator::GetOutputs() const
{
  return this->CC.GetOutputs();
}

std::vector<std::string> const& cmCustomCommandGenerator::GetByproducts() const
{
  return this->CC.GetByproducts();
}

std::vector<std::string> const& cmCustomCommandGenerator::GetDepends() const
{
  if (!this->DependsDone) {
    this->DependsDone = true;
    std::vector<std::string> depends = this->CC.GetDepends();
    for (std::vector<std::string>::const_iterator i = depends.begin();
         i != depends.end(); ++i) {
      cmsys::auto_ptr<cmCompiledGeneratorExpression> cge = this->GE->Parse(*i);
      std::vector<std::string> result;
      cmSystemTools::ExpandListArgument(cge->Evaluate(this->LG, this->Config),
                                        result);
      for (std::vector<std::string>::iterator it = result.begin();
           it != result.end(); ++it) {
        if (cmSystemTools::FileIsFullPath(it->c_str())) {
          *it = cmSystemTools::CollapseFullPath(*it);
        }
      }
      this->Depends.insert(this->Depends.end(), result.begin(), result.end());
    }
  }
  return this->Depends;
}
