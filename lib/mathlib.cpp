/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2012 Daniel Marjamäki and Cppcheck team.
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



#include "mathlib.h"
#include "errorlogger.h"

#include <string>
#include <sstream>
#include <cstdlib>
#include <cmath>
#include <cctype>

MathLib::bigint MathLib::toLongNumber(const std::string &str)
{
    // hexadecimal numbers:
    if (isHex(str)) {
        bigint ret = 0;
        std::istringstream istr(str);
        istr >> std::hex >> ret;
        return ret;
    }

    // octal numbers:
    if (isOct(str)) {
        bigint ret = 0;
        std::istringstream istr(str);
        istr >> std::oct >> ret;
        return ret;
    }

    if (str.find_first_of("eE") != std::string::npos)
        return static_cast<bigint>(std::atof(str.c_str()));

    bigint ret = 0;
    std::istringstream istr(str);
    istr >> ret;
    return ret;
}

double MathLib::toDoubleNumber(const std::string &str)
{
    if (isHex(str))
        return static_cast<double>(toLongNumber(str));
    // nullcheck
    else if (isNullValue(str))
        return 0.0;
    // otherwise, convert to double
    std::istringstream istr(str);
    double ret;
    istr >> ret;
    return ret;
}

bool MathLib::isFloat(const std::string &s)
{
    // every number that contains a . is a float
    if (s.find("." , 0) != std::string::npos)
        return true;
    // scientific notation
    return(s.find("E-", 0) != std::string::npos
           || s.find("e-", 0) != std::string::npos);
}

bool MathLib::isNegative(const std::string &s)
{
    // remember position
    std::string::size_type n = 0;
    // eat up whitespace
    while (std::isspace(s[n])) ++n;
    // every negative number has a negative sign
    return(s[n] == '-');
}

bool MathLib::isOct(const std::string& str)
{
    bool sign = str[0]=='-' || str[0]=='+';
    return(str[sign?1:0] == '0' && (str.size() == 1 || isOctalDigit(str[sign?2:1])) && !isFloat(str));
}

bool MathLib::isHex(const std::string& str)
{
    bool sign = str[0]=='-' || str[0]=='+';
    return(str.compare(sign?1:0, 2, "0x") == 0 || str.compare(sign?1:0, 2, "0X") == 0);
}

bool MathLib::isInt(const std::string & s)
{
    // perform prechecks:
    // ------------------
    // first check, if a point is found, it is an floating point value
    if (s.find(".", 0) != std::string::npos) return false;
    // check for scientific notation e.g. NumberE-Number this is obvious an floating point value
    else if (s.find("E-", 0) != std::string::npos || s.find("e-", 0) != std::string::npos) return false;


    // prechecking has nothing found,...
    // gather information
    enum Representation {
        eScientific = 0, // NumberE+Number or NumberENumber
        eOctal,          // starts with 0
        eHex,            // starts with 0x
        eDefault         // Numbers with a (possible) trailing u or U or l or L for unsigned or long datatypes
    };
    // create an instance
    Representation Mode = eDefault;


    // remember position
    unsigned long n = 0;
    // eat up whitespace
    while (std::isspace(s[n])) ++n;

    // determine type
    if (s.find("E", 0) != std::string::npos) {
        Mode = eScientific;
    } else if (isHex(s)) {
        Mode = eHex;
    } else if (isOct(s)) {
        Mode = eOctal;
    }

    // check sign
    if (s[n] == '-' || s[n] == '+') ++n;

    // check scientific notation
    if (Mode == eScientific) {
        // check digits
        while (std::isdigit(s[n])) ++n;

        // check scientific notation
        if (std::tolower(s[n]) == 'e') {
            ++n;
            // check positive exponent
            if (s[n] == '+') ++n;
            // floating pointer number e.g. 124E-2
            if (s[n] == '-') return false;
            // check digits of the exponent
            while (std::isdigit(s[n])) ++n;
        }
    }
    // check hex notation
    else if (Mode == eHex) {
        ++n; // 0
        ++n; // x
        while (std::isxdigit(s[n]))
            ++n;

        while (std::tolower(s[n]) == 'u' || std::tolower(s[n]) == 'l') ++n; // unsigned or long (long)
    }
    // check octal notation
    else if (Mode == eOctal) {
        ++n; // 0
        while (isOctalDigit(s[n]))
            ++n;

        while (std::tolower(s[n]) == 'u' || std::tolower(s[n]) == 'l') ++n; // unsigned or long (long)
    } else if (Mode == eDefault) {
        // starts with digit
        bool bStartsWithDigit=false;
        while (std::isdigit(s[n])) {
            bStartsWithDigit=true;
            ++n;
        };

        while (std::tolower(s[n]) == 'u' || std::tolower(s[n]) == 'l') ++n; // unsigned or long (long)

        if (!bStartsWithDigit)
            return false;
    }
    // eat up whitespace
    while (std::isspace(s[n]))
        ++n;

    // if everything goes good, we are at the end of the string and no digits/character
    // is here --> return true, but if something was found eg. 12E+12AA return false
    return(n >= s.length());
}

std::string MathLib::add(const std::string & first, const std::string & second)
{
    if (MathLib::isInt(first) && MathLib::isInt(second)) {
        return toString<bigint>(toLongNumber(first) + toLongNumber(second));
    }
    return toString<double>(toDoubleNumber(first) + toDoubleNumber(second));
}

std::string MathLib::subtract(const std::string &first, const std::string &second)
{
    if (MathLib::isInt(first) && MathLib::isInt(second)) {
        return toString<bigint>(toLongNumber(first) - toLongNumber(second));
    }
    return toString<double>(toDoubleNumber(first) - toDoubleNumber(second));
}

std::string MathLib::divide(const std::string &first, const std::string &second)
{
    if (MathLib::isInt(first) && MathLib::isInt(second)) {
        return toString<bigint>(toLongNumber(first) / toLongNumber(second));
    }
    return toString<double>(toDoubleNumber(first) / toDoubleNumber(second));
}

std::string MathLib::multiply(const std::string &first, const std::string &second)
{
    if (MathLib::isInt(first) && MathLib::isInt(second)) {
        return toString<bigint>(toLongNumber(first) * toLongNumber(second));
    }
    return toString<double>(toDoubleNumber(first) * toDoubleNumber(second));
}

std::string MathLib::calculate(const std::string &first, const std::string &second, char action)
{
    switch (action) {
    case '+':
        return MathLib::add(first, second);

    case '-':
        return MathLib::subtract(first, second);

    case '*':
        return MathLib::multiply(first, second);

    case '/':
        return MathLib::divide(first, second);

    default:
        throw InternalError(0, std::string("Unexpected action '") + action + "' in MathLib::calculate(). Please report this to Cppcheck developers.");
    }

    return "0";
}

std::string MathLib::sin(const std::string &tok)
{
    return toString<double>(std::sin(toDoubleNumber(tok)));
}


std::string MathLib::cos(const std::string &tok)
{
    return toString<double>(std::cos(toDoubleNumber(tok)));
}

std::string MathLib::tan(const std::string &tok)
{
    return toString<double>(std::tan(toDoubleNumber(tok)));
}


std::string MathLib::abs(const std::string &tok)
{
    return toString<double>(std::abs(toDoubleNumber(tok)));
}

bool MathLib::isEqual(const std::string &first, const std::string &second)
{
    // this conversion is needed for formating
    // e.g. if first=0.1 and second=1.0E-1, the direct comparison of the strings whould fail
    return toString<double>(toDoubleNumber(first)) == toString<double>(toDoubleNumber(second));
}

bool MathLib::isNotEqual(const std::string &first, const std::string &second)
{
    return !isEqual(first, second);
}

bool MathLib::isGreater(const std::string &first, const std::string &second)
{
    return toDoubleNumber(first) > toDoubleNumber(second);
}

bool MathLib::isGreaterEqual(const std::string &first, const std::string &second)
{
    return toDoubleNumber(first) >= toDoubleNumber(second);
}

bool MathLib::isLess(const std::string &first, const std::string &second)
{
    return toDoubleNumber(first) < toDoubleNumber(second);
}

bool MathLib::isLessEqual(const std::string &first, const std::string &second)
{
    return toDoubleNumber(first) <= toDoubleNumber(second);
}

bool MathLib::isNullValue(const std::string &str)
{
    return (str == "-0"        || str == "0"      || str == "+0"
            || str == "-0.0"   || str == "0.0"    || str == "+0.0"
            || str == "-0."    || str == "+0."
            || str == "-0E-00" || str == "-0E+00" || str == "+0E+00" || str == "+0E-00"
            || str == "-0e-00" || str == "-0e+00" || str == "+0e+00" || str == "+0e-00"
            || str == "-0E-0");
}

bool MathLib::isOctalDigit(char c)
{
    return(c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7');
}
