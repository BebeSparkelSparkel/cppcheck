/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2011 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "cppcheckexecutor.h"
#include "cppcheck.h"
#include "threadexecutor.h"
#include <fstream>
#include <iostream>
#include <cstdlib> // EXIT_SUCCESS and EXIT_FAILURE
#include "cmdlineparser.h"
#include "filelister.h"
#include "path.h"
#include "pathmatch.h"

CppCheckExecutor::CppCheckExecutor()
{
    time1 = 0;
    errorlist = false;
}

CppCheckExecutor::~CppCheckExecutor()
{

}

bool CppCheckExecutor::parseFromArgs(CppCheck *cppcheck, int argc, const char* const argv[])
{
    CmdLineParser parser(&_settings);
    bool success = parser.ParseFromArgs(argc, argv);
    cppcheck->settings(_settings);   // copy the settings

    if (success)
    {
        if (parser.GetShowVersion() && !parser.GetShowErrorMessages())
        {
            std::cout << "Cppcheck " << cppcheck->version() << std::endl;
        }

        if (parser.GetShowErrorMessages())
        {
            errorlist = true;
            std::cout << ErrorLogger::ErrorMessage::getXMLHeader(_settings._xml_version);
            cppcheck->getErrorMessages();
            std::cout << ErrorLogger::ErrorMessage::getXMLFooter(_settings._xml_version) << std::endl;
        }

        if (parser.ExitAfterPrinting())
            std::exit(0);
    }

    // Check that all include paths exist
    {
        std::list<std::string>::iterator iter;
        for (iter = _settings._includePaths.begin();
             iter != _settings._includePaths.end();
            )
        {
            const std::string path(Path::toNativeSeparators(*iter));
            if (FileLister::isDirectory(path))
                ++iter;
            else
            {
                // If the include path is not found, warn user and remove the
                // non-existing path from the list.
                std::cout << "cppcheck: warning: Couldn't find path given by -I '" + path + "'" << std::endl;
                iter = _settings._includePaths.erase(iter);
            }
        }
    }

    std::vector<std::string> pathnames = parser.GetPathNames();
    std::vector<std::string> filenames;
    std::map<std::string, long> filesizes;

    if (!pathnames.empty())
    {
        // Execute recursiveAddFiles() to each given file parameter
        std::vector<std::string>::const_iterator iter;
        for (iter = pathnames.begin(); iter != pathnames.end(); ++iter)
            FileLister::recursiveAddFiles(filenames, filesizes, Path::toNativeSeparators(*iter));
    }

    if (!filenames.empty())
    {
        PathMatch matcher(parser.GetIgnoredPaths());
        std::vector<std::string>::iterator iterBegin = filenames.begin();
        for (int i = (int)filenames.size() - 1; i >= 0; i--)
        {
            if (matcher.Match(filenames[(unsigned int)i]))
                filenames.erase(iterBegin + i);
        }
    }
    else
    {
        std::cout << "cppcheck: error: could not find or open any of the paths given." << std::endl;
        return false;
    }

    if (!filenames.empty())
    {
        std::vector<std::string>::iterator iter;
        for (iter = filenames.begin(); iter != filenames.end(); ++iter)
        {
            _filenames.push_back(*iter);
            _filesizes[*iter] = filesizes[*iter];
        }

        return true;
    }
    else
    {
        std::cout << "cppcheck: error: no files to check - all paths ignored." << std::endl;
        return false;
    }
}

int CppCheckExecutor::check(int argc, const char* const argv[])
{
    CppCheck cppCheck(*this, true);
    if (!parseFromArgs(&cppCheck, argc, argv))
    {
        return EXIT_FAILURE;
    }

    if (cppCheck.settings().reportProgress)
        time1 = std::time(0);

    _settings = cppCheck.settings();
    if (_settings._xml)
    {
        reportErr(ErrorLogger::ErrorMessage::getXMLHeader(_settings._xml_version));
    }

    unsigned int returnValue = 0;
    if (_settings._jobs == 1)
    {
        // Single process

        long totalfilesize = 0;
        for (std::map<std::string, long>::const_iterator i = _filesizes.begin(); i != _filesizes.end(); ++i)
        {
            totalfilesize += i->second;
        }

        long processedsize = 0;
        for (unsigned int c = 0; c < _filenames.size(); c++)
        {
            returnValue += cppCheck.check(_filenames[c]);
            if (_filesizes.find(_filenames[c]) != _filesizes.end())
            {
                processedsize += _filesizes[_filenames[c]];
            }
            if (!_settings._errorsOnly)
                reportStatus(c + 1, _filenames.size(), processedsize, totalfilesize);
        }
    }
    else if (!ThreadExecutor::isEnabled())
    {
        std::cout << "No thread support yet implemented for this platform." << std::endl;
    }
    else
    {
        // Multiple processes
        Settings &settings = cppCheck.settings();
        ThreadExecutor executor(_filenames, _filesizes, settings, *this);
        returnValue = executor.check();
    }

    reportUnmatchedSuppressions(cppCheck.settings().nomsg.getUnmatchedGlobalSuppressions());

    if (_settings._xml)
    {
        reportErr(ErrorLogger::ErrorMessage::getXMLFooter(_settings._xml_version));
    }

    if (returnValue)
        return _settings._exitCode;
    else
        return 0;
}

void CppCheckExecutor::reportErr(const std::string &errmsg)
{
    std::cerr << errmsg << std::endl;
}

void CppCheckExecutor::reportOut(const std::string &outmsg)
{
    std::cout << outmsg << std::endl;
}

void CppCheckExecutor::reportProgress(const std::string &filename, const char stage[], const unsigned int value)
{
    (void)filename;

    if (!time1)
        return;

    // Report progress messages every 10 seconds
    const std::time_t time2 = std::time(NULL);
    if (time2 >= (time1 + 10))
    {
        time1 = time2;

        // current time in the format "Www Mmm dd hh:mm:ss yyyy"
        const std::string str(std::ctime(&time2));

        // format a progress message
        std::ostringstream ostr;
        ostr << "progress: "
             << stage
             << " " << int(value) << "%";
        if (_settings._verbose)
            ostr << " time=" << str.substr(11, 8);

        // Report progress message
        reportOut(ostr.str());
    }
}

void CppCheckExecutor::reportStatus(unsigned int fileindex, unsigned int filecount, long sizedone, long sizetotal)
{
    if (filecount > 1)
    {
        std::ostringstream oss;
        oss << fileindex << "/" << filecount
            << " files checked " <<
            (sizetotal > 0 ? static_cast<long>(static_cast<double>(sizedone) / sizetotal*100) : 0)
            << "% done";
        std::cout << oss.str() << std::endl;
    }
}

void CppCheckExecutor::reportErr(const ErrorLogger::ErrorMessage &msg)
{
    if (errorlist)
    {
        reportOut(msg.toXML(false, _settings._xml_version));
    }
    else if (_settings._xml)
    {
        reportErr(msg.toXML(_settings._verbose, _settings._xml_version));
    }
    else
    {
        reportErr(msg.toString(_settings._verbose, _settings._outputFormat));
    }
}
