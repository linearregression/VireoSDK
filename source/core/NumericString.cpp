/**

Copyright (c) 2014 National Instruments Corp.
 
This software is subject to the terms described in the LICENSE.TXT file

SDG
*/

/*! \file
    \brief Native Verio VIA functions.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include "TypeDefiner.h"
#include "ExecutionContext.h"
#include "StringUtilities.h"
#include "TDCodecVia.h"

#if !(kVireoOS_win32U || kVireoOS_win64U )
    #include <math.h>
#endif

namespace Vireo
{
//------------------------------------------------------------
struct FormatOptions {
    Int32 ArgumentOrder; // 3$, 2$ negative number means default order
    Boolean RemoveTrailing; // #
    Boolean Valid;
    Boolean LeftJustify;
    Boolean ShowSign;           // + or - always
    Boolean SignPad;            // ' ' for positive '-' for negative
    Boolean BasePrefix;         // 0, 0x, or 0X
    Boolean ZeroPad;            // 00010 '0'
    Boolean VariablePrecision;
    char    FormatChar;         // my affect output 'x' or 'X'
    char OriginalFormatChar;
    char DecimalSeparator;
    char NumericLength[3];
    Boolean EngineerNotation;
    Int32   MinimumFieldWidth;  // If zero no padding
    Int32   Precision; //.3
    Int32   Significant; //_4
    SubString  FmtSubString;
    Boolean ConsumeArgument;
};
//------------------------------------------------------------
void ReadPercentFormatOptions(SubString *format, FormatOptions *pOptions)
{
    // Derived on the specification found here.
    // http://www.cplusplus.com/reference/cstdio/printf/
    // There will be some allowances for LabVIEW and since
    // data is typed codes that identify type size like
    // (hh, ll j, z, r, and L) are not needed.

    pOptions->ShowSign = false;
    pOptions->LeftJustify = false;
    pOptions->ZeroPad = false;
    pOptions->BasePrefix = false;
    pOptions->SignPad = false;
    pOptions->VariablePrecision = false;
    pOptions->MinimumFieldWidth = 0;
    pOptions->Precision = -1;
    pOptions->ArgumentOrder = -1;
    pOptions->RemoveTrailing = false;
    pOptions->Significant = -1;
    pOptions->NumericLength[0] = '\0';
    pOptions->NumericLength[1] = '\0';
    pOptions->NumericLength[2] = '\0';
    pOptions->EngineerNotation = false;
    pOptions->ConsumeArgument = true;
    Boolean bPrecision = false;
    Boolean bValid = true;
    Utf8Char c;
    const Utf8Char* pBegin = format->Begin();

    while (bValid && format->ReadRawChar(&c)) {

        SubString order("$");
        SubString percent("%");

        if (strchr("diuoxXfFeEgGaAcsptTbB%", c)) {
            pOptions->FormatChar = c;
            break;
        }
        if (c == '[') {
            // only used for scan from string related function
            SubString charSet("]");
            IntIndex charSetIndex = format->FindFirstMatch(&charSet, 0, false);
            if (charSetIndex < 0) {
                bValid = false;
            } else {
                format->AliasAssign(format->Begin()+charSetIndex+1, format->End());
                pOptions->FormatChar = '[';
            }
            break;
        }
        if (c == '+') {
            pOptions->ShowSign = true;
        } else if (c == '-') {
            pOptions->LeftJustify = true;
        } else if (c == '#') {
            pOptions->BasePrefix = true;
            pOptions->RemoveTrailing = true;
        } else if (c == ' ') {
            // space flag not used in LabView
            pOptions->SignPad = true;
        } else if (c == '^') {
            pOptions->EngineerNotation = true;
        } else if (c == '.') {
            bPrecision = true;
            format->AliasAssign(format->Begin(), format->End());
            IntMax value = 0;
            if (format->ReadInt(&value)) {
                pOptions->Precision = (Int32)value;
            } else {
            	bValid = false;
            }
        } else if (c == '_') {
            bPrecision = true;
            format->AliasAssign(format->Begin(), format->End());
            IntMax value = 0;
            if (format->ReadInt(&value)) {
                pOptions->Significant = (Int32)value;
            }
        } else if (bPrecision && c == '*') {
            // Labview doesn't variable precision, it will mess up with the argument index.
            pOptions->VariablePrecision = true;
        } else if (c == '$') {
        } else if (c == ';') {
            // local conversion code
        } else if (strchr("hl", c)) {
            if(format->Length()<=0) {
                bValid = false;
                break;
            } else {
                Utf8Char nextC = *(format->Begin());
                if (strchr("duoxXfFeEgGpbB", nextC)) {
                    pOptions->NumericLength[0] = c;
                } else {
                    bValid = false;
                    break;
                }
            }
        } else {
            IntIndex orderIndex = format->FindFirstMatch(&order, 0, false);
            if ((c >= '0' && c <= '9') && orderIndex>=0) {
                format->AliasAssign(format->Begin()-1, format->End());
                IntMax value = 0;
                if (format->ReadInt(&value)) {
                    pOptions->ArgumentOrder = (Int32)value;
                }
            } else if (c == '0') {
                pOptions->ZeroPad = true;
            } else if (c >= '0' && c <= '9') {
                // Back up and read the whole number.
                format->AliasAssign(format->Begin()-1, format->End());
                IntMax value = 0;
                if (format->ReadInt(&value)) {
                    pOptions->MinimumFieldWidth = (Int32) value;
                }
             } else {
                 bValid = false;
                 break;
             }
        }
    }
    pOptions->Valid = bValid;
    if (!pOptions->Valid) {
        pOptions->FormatChar = '0';
    }
    pOptions->ConsumeArgument = (pOptions->FormatChar != '%') && (pOptions->FormatChar != ';');
    pOptions->OriginalFormatChar = pOptions->FormatChar;
    pOptions->FmtSubString.AliasAssign(pBegin, format->Begin());
}
//---------------------------------------------------------------------------------------------
void GenerateFinalNumeric (const FormatOptions* , char* , Int32* , TempStackCString* , Boolean );
void RefactorLabviewNumeric(const FormatOptions* , char* , Int32* , Int32 , Int32 );

void DefaultFormatCode(Int32 count, StaticTypeAndData arguments[], TempStackCString* buffer)
{
    Int32 index = 0;
    for (Int32 i=0; i< count; i++) {
        if(i!=0) {
            buffer->AppendCStr(" ");
            index++;
        }
        TypeRef argType = arguments[i]._paramType;
        switch(argType->BitEncoding()) {

        case kEncoding_UInt: {
            buffer->AppendCStr("%u");
        }
        break;
        case kEncoding_SInt:
        case kEncoding_MetaInt: {
            buffer->AppendCStr("%d");
        }
        break;
        case kEncoding_IEEE754Binary: {
            buffer->AppendCStr("%f");
        }
        break;
        case kEncoding_Array: {
            TypedArrayCoreRef* pArray = (TypedArrayCoreRef*)(arguments[i]._pData);
            TypeRef elementType = (*pArray)->ElementType();
            EncodingEnum elementEncoding = elementType->BitEncoding();
            if (argType->Rank()==1 && (elementEncoding == kEncoding_Ascii || elementEncoding == kEncoding_Unicode)) {
                buffer->AppendCStr("%s");
            } else {
                // doesn't support yet
                buffer->AppendCStr("%Z");
            }
        }
        break;
        default:
            buffer->AppendCStr("%Z");
        break;
        }
    }
}
/*
 * Handle the Integer overflow in Labview.
 * */
void ConvertNumericType(Int32 size, Boolean unsign, IntMax input, IntMax* output)
{
    IntMax NumericLimit[12] = {0, 255, -127, 128, 0, 65536, -32768, 32767, 0, 4294967295, -2147483648, 2147483647};
    int limitIndex = 0;
    if (size < 8) {
        switch (size) {
        case 1:
            limitIndex = 0;
        break;
        case 2:
            limitIndex = 1;
        break;
        case 4:
            limitIndex = 2;
        break;
        default:
            limitIndex = 0;
        }
        if (!unsign) {
            limitIndex = limitIndex*4 + 2;
        } else {
            limitIndex = limitIndex*4;
        }
        if (input<NumericLimit[limitIndex]) {
            input = NumericLimit[limitIndex];
        } else if (input>NumericLimit[limitIndex+1]) {
            input = NumericLimit[limitIndex+1];
        }
        *output = input;
    }
    // do nothing for int64 and uint64. We may not reach the max uint64 now.
}

void Format(SubString *format, Int32 count, StaticTypeAndData arguments[], StringRef buffer)
{
    IntIndex argumentIndex = 0;
    Boolean lastArgumentSpecified = false;
    IntIndex lastArgumentIndex = -1;
    IntIndex explicitPositionArgument = 0;
    Int32 totalArgument = 0;;
    char activeDecimalPoint = '.';
    SubString f(format);            // Make a copy to use locally

    buffer->Resize1D(0);              // Clear buffer (wont do anything for fixed size)
    Boolean validFormatString = true;
    Utf8Char c = 0;

    while (validFormatString && f.ReadRawChar(&c))
    {
        if (c == '\\' && f.ReadRawChar(&c)) {
            switch (c)
            {
                case 'n':       buffer->Append('\n');      break;
                case 'r':       buffer->Append('\r');      break;
                case 't':       buffer->Append('\t');      break;
                case 'b':       buffer->Append('\b');      break;
                case 'f':       buffer->Append('\f');      break;
                case 's':       buffer->Append(' ');       break;
                case '\\':      buffer->Append('\\');      break;
                default:  break;
            }
        } else if (c == '%') {

            FormatOptions fOptions;
            ReadPercentFormatOptions(&f, &fOptions);
            // We should assign the local decimal point to DecimalSeparator.
            fOptions.DecimalSeparator = activeDecimalPoint;
            totalArgument++;
            if (lastArgumentIndex == argumentIndex) {
                // the previous argument is a legal argument. like %12$%
                totalArgument --;
                if (lastArgumentSpecified) {
                    explicitPositionArgument --;
                }
            }
            lastArgumentSpecified = false;
            argumentIndex = totalArgument-explicitPositionArgument-1;
            if (fOptions.ArgumentOrder>=0) {
                if (fOptions.ArgumentOrder > 0 ) {
                    argumentIndex = fOptions.ArgumentOrder-1;
                    explicitPositionArgument ++;
                    lastArgumentSpecified = true;
                }
                SubString *fmtSubString = &fOptions.FmtSubString;
                fmtSubString->AliasAssign(fmtSubString->Begin(), fmtSubString->End());
                SubString order("$");
                IntIndex dollarFlag = fmtSubString->FindFirstMatch(&order, 0, false);
                fmtSubString->AliasAssign(fmtSubString->Begin()+ dollarFlag + 1, fmtSubString->End());
            }
            lastArgumentIndex = argumentIndex;
            Boolean parseFinished = false;
            if (!fOptions.Valid) {
                parseFinished = true;
                validFormatString = false;
            } else if (argumentIndex > count-1 && fOptions.ConsumeArgument) {
                validFormatString = false;
                parseFinished = true;
            }

            while (!parseFinished){
                parseFinished = true;
                switch (fOptions.FormatChar)
                {
                    case 'g': case 'G':
                    {
                        // will re-parse the format later with new format code
                        parseFinished = false;
                        Double tempDouble = *(Double*) (arguments[argumentIndex]._pData);
                        Int32 exponent = 0;
                        Int32 precision = fOptions.Precision;
                        if (precision<0) {
                               // 6 is the default value;
                               precision = 6;
                        }
                        if (tempDouble != 0.0) {
                            Double absDouble = tempDouble;
                            if (tempDouble < 0) {
                            absDouble = 0.0 - tempDouble;
                            }
                            exponent = floor(log10(absDouble));
                        }
                        if (fOptions.Significant >= 0) {
                            precision = fOptions.Significant-1;
                            if (precision < 0) {
                                precision = 0;
                            }
                        }
                        if (exponent >= -4 && exponent <= precision) {
                            fOptions.FormatChar = 'f';
                        } else {
                            fOptions.FormatChar = 'e';
                        }
                    }
                    break;
                    case 'f': case 'F':
                    {
                        Double tempDouble;
                        TypeRef argType = arguments[argumentIndex]._paramType;
                        ReadDoubleFromMemory(argType->BitEncoding(), argType->TopAQSize(),  arguments[argumentIndex]._pData, &tempDouble);
                        Int32 leadingZero = 0;
                        Int32 exponent = 0;
                        Int32 precision = fOptions.Precision;
                        Int32 truncateSignificant = 0;
                        // calculate the exponent of the number, it also tell us whether should truncate the integer part.
                        if (fOptions.Significant >= 0) {
                            if (tempDouble != 0) {
                                Double absDouble = tempDouble;
                                if (tempDouble < 0) {
                                    absDouble = 0.0 - tempDouble;
                                }
                                exponent = floor(log10(absDouble));
                            }
                            // 0.12 has 1 leading zero
                            leadingZero = (exponent >= 0)? 0 : (0 - exponent);
                            precision = (exponent >= 0)? (fOptions.Significant - exponent - 1) : (fOptions.Significant + leadingZero - 1);
                            if (precision < 0) {
                                precision = 0;
                                truncateSignificant = exponent + 1 - fOptions.Significant;
                                // need truncate the integer part of the float because the sprintf doesnt do this for us.
                            }
                        }
                        char asciiReplacementString[2*stringBufferSize];
                        Int32 sizeOfNumericString = -1;
                        char formatCode[10];
                        if (precision >= 0) {
                            sprintf(formatCode, "%%.*%sf", fOptions.NumericLength);
                            // formatCode : %.*hf
                            sizeOfNumericString = snprintf(asciiReplacementString, stringBufferSize, formatCode, precision,tempDouble);
                        } else {
                            sprintf(formatCode, "%%%sf", fOptions.NumericLength);
                            // formatCode: %hf
                            sizeOfNumericString = snprintf(asciiReplacementString, stringBufferSize, formatCode, tempDouble);
                        }
                        Int32 intDigits = (exponent >= 0)? (exponent): 0 ;
                        TempStackCString paddingPart;
                        RefactorLabviewNumeric(&fOptions, asciiReplacementString, &sizeOfNumericString, intDigits, truncateSignificant);
                        buffer->Append(sizeOfNumericString, (Utf8Char*)asciiReplacementString);
                        argumentIndex++;
                    }
                    break;
                    case 'e': case 'E':
                    {
                        Double tempDouble;
                        TypeRef argType = arguments[argumentIndex]._paramType;
                        ReadDoubleFromMemory(argType->BitEncoding(), argType->TopAQSize(),  arguments[argumentIndex]._pData, &tempDouble);
                        Int32 precision = fOptions.Precision;
                        if (precision>0 && fOptions.EngineerNotation) {
                            Int32 exponent = 0;
                            if (tempDouble != 0) {
                                Double absDouble = tempDouble;
                                if (tempDouble < 0) {
                                    absDouble = 0.0 - tempDouble;
                                }
                                exponent = floor(log10(absDouble));
                            }
                            if (exponent%3 != 0) {
                                precision = (exponent>=0)? precision + exponent%3 : precision + 3+exponent%3;
                            }
                        }
                        if (fOptions.Significant >= 0) {
                            precision =  fOptions.Significant - 1;
                        }
                        char asciiReplacementString[2*stringBufferSize];
                        Int32 sizeOfNumericString = 0;
                        char formatCode[10];
                        if (precision >= 0) {
                            sprintf(formatCode, "%%.*%se", fOptions.NumericLength);
                            // formatCode : %.*he
                            sizeOfNumericString = snprintf(asciiReplacementString, stringBufferSize, "%.*e", precision, tempDouble);
                        } else {
                            sprintf(formatCode, "%%%se", fOptions.NumericLength);
                            // formatCode : %he
                            sizeOfNumericString = snprintf(asciiReplacementString, stringBufferSize, "%e", tempDouble);
                        }
                        RefactorLabviewNumeric(&fOptions, asciiReplacementString, &sizeOfNumericString, 0, 0);

                        buffer->Append(sizeOfNumericString, (Utf8Char*)asciiReplacementString);
                        argumentIndex++;
                    }
                    break;
                    case 'p': case 'P':
                    {
                        parseFinished = false;
                        fOptions.OriginalFormatChar = 'p';
                        fOptions.FormatChar = 'e';
                        fOptions.EngineerNotation = true;
                    }
                    break;
                    case 'a': case 'A':
                    {
                        // TODO don't assume data type. This just becomes the default format for real numbers, then use formatter
                        SubString percentFormat(fOptions.FmtSubString.Begin()-1, fOptions.FmtSubString.End());
                        TempStackCString tempFormat(&percentFormat);
                        char asciiReplacementString[stringBufferSize];
                        //Get the numeric string that will replace the format string
                        Double tempDouble = *(Double*) (arguments[argumentIndex]._pData);
                        Int32 sizeOfNumericString = snprintf(asciiReplacementString, stringBufferSize, tempFormat.BeginCStr(), tempDouble);
                        buffer->Append(sizeOfNumericString, (Utf8Char*)asciiReplacementString);
                        argumentIndex++;
                    }
                    break;
                    case 'b': case 'B':
                    {
                        fOptions.FormatChar = 'B';
                        SubString percentFormat(fOptions.FmtSubString.Begin()-1, fOptions.FmtSubString.End());
                        TempStackCString formattedNumber;
                        TypeRef argType = arguments[argumentIndex]._paramType;
                        IntMax intValue;
                        Int32 intSize = 8*argType->TopAQSize();

                        ReadIntFromMemory(argType->BitEncoding(), argType->TopAQSize(), arguments[argumentIndex]._pData, &intValue);
                        char BinaryString[2*stringBufferSize];
                        char bits [2];
                        bits[0] = '0';
                        bits[1] = '1';
                        Int32 length = 0;
                        if (intValue < 0) {
                            for (int i = intSize-1; i >=0; i--) {
                                if (intValue%2 == 0) {
                                    BinaryString[i] = '0';
                                } else {
                                    BinaryString[i] = '1';
                                }
                                length++;
                                intValue = intValue >> 1;
                            }
                        } else {
                            if (intValue == 0) {
                                BinaryString[0] = bits[intValue];
                                length = 1;
                            }
                            while (intValue >= 1) {
                                    BinaryString[intSize-1-length] =  bits[intValue%2];
                                    intValue = intValue/2;
                                    length++;
                            }
                        }
                        char* binaryindex = BinaryString;
                        Int32 binaryStringLength = length;

                        RefactorLabviewNumeric(&fOptions, binaryindex, &binaryStringLength, 0, 0);
                        buffer->Append(binaryStringLength, (Utf8Char*)binaryindex);
                        argumentIndex++;
                    }
                    break;
                    case 'd':
                    case 'o': case 'u':
                    case 'x': case 'X':
                    {
                        // To cover the max range formats like %d ned to beturned into %lld
                        SubString percentFormat(fOptions.FmtSubString.Begin()-1, fOptions.FmtSubString.End());
                        TempStackCString tempFormat((Utf8Char*)"%", 1);
                        SubString *fmtSubString = &fOptions.FmtSubString;
                        fmtSubString->AliasAssign(fmtSubString->Begin(), fmtSubString->End()-1);
                        tempFormat.Append(fmtSubString);
                        char specifier[] = "d";
                        if (fOptions.FormatChar == 'x') {
                            fOptions.FormatChar = 'X';
                        }
                        if (fOptions.FormatChar == 'U') {
                            fOptions.FormatChar = 'u';
                        }
                        specifier[0] = fOptions.FormatChar;
                        tempFormat.AppendCStr(specifier);

                        TempStackCString formattedNumber;
                        TypeRef argType = arguments[argumentIndex]._paramType;
                        IntMax intValue;
                        if (argType->BitEncoding() == kEncoding_IEEE754Binary) {
                            if(fOptions.FormatChar == 'u') {
                                ReadIntFromMemory(argType->BitEncoding(), 8, arguments[argumentIndex]._pData, &intValue);
                                ConvertNumericType(4, true, intValue, &intValue);
                            } else {
                                ReadIntFromMemory(argType->BitEncoding(), 8, arguments[argumentIndex]._pData, &intValue);
                                ConvertNumericType(4, false, intValue, &intValue);
                            }
                        } else {
                            ReadIntFromMemory(argType->BitEncoding(), argType->TopAQSize(), arguments[argumentIndex]._pData, &intValue);
                        }
                        Int32 length = snprintf(formattedNumber.BeginCStr(), formattedNumber.Capacity(), tempFormat.BeginCStr(), intValue);
                        buffer->Append(length, (Utf8Char*)formattedNumber.Begin());

                        argumentIndex++;
                    }
                    break;
                    case '%':      //%%
                    buffer->Append('%');
                    break;
                    case 's':      //%s
                    {
                        STACK_VAR(String, tempString);
                        TDViaFormatter formatter(tempString.Value, false);
                        formatter.FormatData(arguments[argumentIndex]._paramType, arguments[argumentIndex]._pData);

                        Int32 extraPadding = fOptions.MinimumFieldWidth - tempString.Value->Length();

                        if (fOptions.LeftJustify)
                        buffer->Append(tempString.Value);
                        if (extraPadding > 0) {
                            for (Int32 i = extraPadding; i >0; i--) {
                                buffer->Append(' ');
                            }
                        }
                        if (!fOptions.LeftJustify)
                        buffer->Append(tempString.Value);

                        argumentIndex++;
                    }
                    break;
                    default:
                    printf("special error character %c\n",fOptions.FormatChar );
                    // This is just part of the format specifier, let it become part of the percent format
                    break;
                }
            }
        } else {
            buffer->Append(c);
        }
    }
}

/* Adjust the numeric string.
 * 1. truncate the integer part if necessary for %f. %_2f   1345.55
 * 2. calculate the significant digits to guarantee the correctness.  %_2f 9.9
 * 3. Apply the engineer notation which means adjust the base and exponent part. %^_3e
 * 4. Remove the trailing zero if necessary. %#3f 1.0000
 * 5. Replace the C decimal pointer with the local decimal separator
 * Input of this function is like "-12.4" "7.450E+043"
 * This function will not process the sign and padding and the width. It only process the pure number.
 * */
void RefactorLabviewNumeric(const FormatOptions* formatOptions, char* bufferBegin, Int32* pSize, Int32 IntDigits, Int32 truncateSignificant)
{
    Boolean negative = false;
    char* buffer = bufferBegin;

    // the positive number string always start from the beginning
    Int32 numberStart = 0;
    Int32 numberEnd = *pSize - 1;
    Int32 decimalPoint = -1;
    Int32 exponentPos = -1;
    Int32 index = 0;
    Int32 size = *pSize;
    if (strchr ("DdoXxbB", formatOptions->FormatChar)) {
        decimalPoint = 0;
        exponentPos = 0;
    }
    if (strchr ("fF", formatOptions->FormatChar)) {
        exponentPos = 0;
    }
    if (*(buffer + numberStart) == '-') {
        negative = true;
        numberStart++;
    }
    while (!(decimalPoint >= 0 && exponentPos >= 0) && index < size) {
        char digit = *(buffer+index);
        if (digit == '.') {
            decimalPoint = index;
        } else if (digit == 'E' || digit == 'e') {
             exponentPos = index;
         }
        index++;
    }
    if (decimalPoint < 0) {
        decimalPoint = 0;
    }

    if (formatOptions->FormatChar == 'f' || formatOptions->FormatChar == 'F') {
        if (truncateSignificant>0) {
            // .0 in sprintf. no decimal point,
            // but still truncate the integer part which is not handled in sprintf

            Int32 trailing = numberStart + formatOptions->Significant;
            if (decimalPoint > 0 && numberStart + truncateSignificant >= decimalPoint) {
                trailing = numberStart + formatOptions->Significant + 1;
            }
            Boolean extend = false;
            if (*(buffer+trailing) > '5') {
                // LabVIEW typically uses Bankers rounding, but for
                // significant digits it always round midpoints down.
                *(buffer+trailing-1) = *(buffer+trailing-1) + 1;
            }
            for (Int32 i = trailing-1; i >= numberStart; i++) {
                if (*(buffer+i) > '9') {
                    *(buffer+i) = '0';
                    if (i == numberStart) {
                        extend =true;
                        break;
                    }
                    *(buffer+i-1) = *(buffer+i-1) +1 ;
                } else {
                    break;
                }
            }
            // It's guaranteed that the length of the float number doesn't change.
            for (Int32 i = trailing; i <= numberEnd; i++) {
                *(buffer+i) = '0';
            }
            if (extend) {
                for (Int32 i = numberEnd; i > numberStart; i--) {
                    *(buffer+i) = *(buffer+i-1);
                }
                *(buffer+ numberStart) =  '1';
            }
        } else if (IntDigits+1 < decimalPoint - numberStart && formatOptions->Significant >= 0) {
            // generate extra significant digit at MSB.
            // There may be a decimal point in the string and the snprintf may generate another digit when rounding.
            // need to fix the redundancy digit

            *(buffer+numberEnd) = ' ';
            if (*(buffer+numberEnd-1) == '.') {
                *(buffer+numberEnd-1) = ' ';
                decimalPoint = 0;
                numberEnd--;
            }
            numberEnd--;
        }
        if (formatOptions->RemoveTrailing) {
            // dont remove the first zero for number 0
            while ((*(buffer+numberEnd)== '0' || *(buffer+numberEnd)== '.') && numberEnd > numberStart) {
                numberEnd--;
            }
        }
        TempStackCString numberPart((Utf8Char*)buffer+ numberStart, numberEnd + 1 - numberStart);
        GenerateFinalNumeric(formatOptions, bufferBegin, pSize, &numberPart, negative);
    } else if (formatOptions->FormatChar == 'E' || formatOptions->FormatChar == 'e') {
        char siPrefixesTable[] = {'y', 'z', 'a', 'f', 'p', 'n','u', 'm', '0', 'k', 'M','G', 'T', 'P','E', 'Z', 'Y'};
        Int32 numberIndex = numberStart;
        Int32 baseIndex = 0;
        // baseIndex used to traverse the tempNumber char array.
        SubString ScientificFloat((Utf8Char*)buffer+exponentPos+1, (Utf8Char*)buffer+numberEnd+1);
        IntMax exponent;
        ScientificFloat.ReadInt(&exponent);
        Int32 paddingBase = exponent%3;
        if (formatOptions->EngineerNotation &&  (paddingBase%3 != 0)) {

            if (paddingBase < 0) {
                paddingBase += 3;
            }
            char tempNumber[stringBufferSize];
            exponent = exponent - paddingBase;

            // we are lucky, this case will never generate extra significant digit at MSB.

            tempNumber[baseIndex] = *(buffer + numberIndex);
            baseIndex++;
            numberIndex ++;
            while (baseIndex <= paddingBase) {
                Utf8Char movedChar = '0';
                if (*(buffer + numberIndex)== '.') {
                    numberIndex++;
                }
                if (*(buffer + numberIndex) != 'e') {
                       movedChar = *(buffer + numberIndex);
                   } else {
                       numberIndex--;
                   }
                tempNumber[baseIndex] = movedChar;
                baseIndex ++;
                numberIndex++;
            }
            if (*(buffer + numberIndex) != 'e') {
                tempNumber[baseIndex] = formatOptions->DecimalSeparator;
                baseIndex ++;
            }

            while (*(buffer + numberIndex) != 'e') {
                tempNumber[baseIndex] = *(buffer + numberIndex);
                baseIndex ++;
                numberIndex++;
            }
            if (formatOptions->RemoveTrailing) {
                while ((tempNumber[baseIndex-1]=='0' || tempNumber[baseIndex-1]==formatOptions->DecimalSeparator) && baseIndex > 1) {
                    baseIndex --;
                }
            }
            // add support for %p
            if (exponent >= -24 && exponent <= 24 && (formatOptions->OriginalFormatChar == 'p')) {

                Int32 siIndex = (Int32)((exponent+24)/3);
                // Attention: -2 --- +2 will not be used
                if (siPrefixesTable[siIndex] != '0' ){
                    tempNumber[baseIndex] = siPrefixesTable[siIndex];
                    baseIndex ++;
                }

            } else {
                // we can use %d safely, because the exponent part is never long than Int32 in double
                Int32 sizeOfExpoent = snprintf(tempNumber + baseIndex, stringBufferSize-baseIndex, "E%+d", (Int32)exponent);
                baseIndex += sizeOfExpoent;
            }
            TempStackCString numberPart((Utf8Char*)tempNumber, baseIndex);
            GenerateFinalNumeric(formatOptions, bufferBegin, pSize, &numberPart, negative);

        } else {
            char tempNumber[stringBufferSize];
            baseIndex = 0;
            for (Int32 i = numberStart; i<exponentPos; i++) {
                tempNumber[baseIndex] = *(buffer+i);
                baseIndex ++;
            }
            if (formatOptions->RemoveTrailing) {
                while ((tempNumber[baseIndex-1]=='0' || tempNumber[baseIndex-1]==formatOptions->DecimalSeparator) && baseIndex > 1) {
                    baseIndex --;
                }
            }
            if (exponent>=-24 && exponent<=24 && (formatOptions->OriginalFormatChar == 'p')) {

                Int32 siIndex = (Int32)((exponent+24)/3);
                // Attention: -2 --- +2 will not be used
                if (siPrefixesTable[siIndex] != '0' ){
                    tempNumber[baseIndex] = siPrefixesTable[siIndex];
                    baseIndex ++;
                }
            } else {
                Int32 sizeOfExpoent = snprintf(tempNumber + baseIndex, stringBufferSize-baseIndex, "E%+d", (Int32)exponent);
                baseIndex += sizeOfExpoent;
            }
            TempStackCString numberPart((Utf8Char*)tempNumber, baseIndex);
            GenerateFinalNumeric(formatOptions, bufferBegin, pSize, &numberPart, negative);
        }
    } else if (formatOptions->FormatChar == 'B' || formatOptions->FormatChar == 'b') {
        TempStackCString numberPart((Utf8Char*)bufferBegin, *pSize);
        GenerateFinalNumeric(formatOptions, bufferBegin, pSize, &numberPart, false);
    }
}

/* This function will calculate the length and fill the numeric string if necessary.
 *
 * */
void GenerateFinalNumeric (const FormatOptions* formatOptions, char* bufferBegin, Int32* pSize, TempStackCString* numberPart, Boolean negative)
{
    // the input buffer is pure numeric. will generate the final format numeric with '+' or padding zero.
    TempStackCString leadingPart;
    Int32 width = formatOptions->MinimumFieldWidth;

    if (!negative) {
        if (formatOptions->ShowSign) {
            leadingPart.AppendCStr("+");
        } else if (formatOptions->SignPad) {
            leadingPart.AppendCStr(" ");
        }
    } else {
        if(formatOptions->FormatChar == 'B' || formatOptions->FormatChar == 'X') {
            leadingPart.AppendCStr("+");
        } else {
            leadingPart.AppendCStr("-");
        }
    }
    if (formatOptions->LeftJustify) {
        width = width - leadingPart.Length();
        width = width>0? width : 0;
        *pSize = snprintf(bufferBegin, stringBufferSize, "%s%-*s", leadingPart.BeginCStr(), width, numberPart->BeginCStr());
    } else {
        // calculate the padding
        width = width - leadingPart.Length();
        width = width - numberPart->Length();
        if (width <=0 ) {
            *pSize = snprintf(bufferBegin, stringBufferSize, "%s%s", leadingPart.BeginCStr(), numberPart->BeginCStr());
        } else {
            if (formatOptions->ZeroPad) {
                *pSize = snprintf(bufferBegin, stringBufferSize, "%s%0*d%s", leadingPart.BeginCStr(), width, 0, numberPart->BeginCStr());
            } else {
                *pSize = snprintf(bufferBegin, stringBufferSize, "%*s%s%s", width, " ", leadingPart.BeginCStr(), numberPart->BeginCStr());
            }
         }
    }
}
//--------------------------------------------------------------------------------------------
Boolean BelongtoCharSet(SubString* charSet, Utf8Char candidate) {
    if (charSet->Length() == 0) {
        return false;
    }
    IntIndex i = 0;
    Utf8Char* begin = (Utf8Char*)charSet->Begin();
    while (i< charSet->Length()) {
        if (i+2< charSet->Length() && (*(begin + 1 + i)=='-')) {
            IntIndex range = *(begin + 2 + i) - *(begin+i);

            for( IntIndex j = 0; j<= range; j++) {
                Utf8Char rangeChar = *(begin + i)+j;
                if (candidate == rangeChar) {
                    return true;
                } else {
                }
            }
            i= i+3;
        } else {
            Utf8Char c = *(charSet->Begin()+i);
            if (c == candidate) {
                return true;
            }
            i++;
        }
    }
    return false;
}
//----------------------------------------------------------------------------------------------------
Boolean TypedScanString(SubString* inputString, IntIndex* endToken, const FormatOptions* formatOptions, StaticTypeAndData* argument, TempStackCString* formatString)
{
    SubString in(inputString);
    TempStackCString truncateInput;
    if (formatOptions->MinimumFieldWidth > 0) {
        IntIndex leadingSpace = 0;
        for (IntIndex i =0; i< in.Length(); i++) {
            if (isspace(*(in.Begin()+i))) {
                leadingSpace++;
            } else {
                break;
            }
        }
        in.AliasAssign(in.Begin(), in.Begin()+formatOptions->MinimumFieldWidth+leadingSpace);
    }
    truncateInput.Append(&in);
    char* inpBegin = truncateInput.BeginCStr();
    char* endPointer = null;
    TypeRef argumentType = argument->_paramType;
    EncodingEnum encoding = argumentType->BitEncoding();
    switch (encoding) {
    case kEncoding_UInt: {
        IntMax intValue;
        switch (formatOptions->FormatChar) {
        case 'x' : {
            intValue = strtoull(inpBegin, &endPointer, 16);
        }
            break;
        case 'd' : case 'u' :{
            intValue = strtoull(inpBegin, &endPointer, 10);
        }
            break;
        case 'b' : {
            intValue = strtoull(inpBegin, &endPointer, 2);
        }
            break;
        case 'o' : {
            intValue = strtoull(inpBegin, &endPointer, 8);
        }
            break;
        default:
            intValue = strtoull(inpBegin, &endPointer, 10);
            break;
        }
        WriteIntToMemory(argumentType->BitEncoding(), argumentType->TopAQSize(), argument->_pData, intValue);
    }
    break;
    case kEncoding_SInt:
    case kEncoding_MetaInt: {
        IntMax intValue = 0;
        switch (formatOptions->FormatChar) {
        case 'x' : {
            intValue = strtoll(inpBegin, &endPointer, 16);
        }
            break;
        case 'd' : case 'u' :{
            intValue = strtoll(inpBegin, &endPointer, 10);
        }
            break;
        case 'b' : case 'B' : {
            intValue = strtoll(inpBegin, &endPointer, 2);
        }
            break;
        case 'o' : {
            intValue = strtoll(inpBegin, &endPointer, 8);
        }
            break;
        default:
            intValue = strtoll(inpBegin, &endPointer, 10);
            break;
        }
        WriteIntToMemory(argumentType->BitEncoding(), argumentType->TopAQSize(), argument->_pData, intValue);
    }
    break;
    case kEncoding_IEEE754Binary: {
        double doubleValue;
        doubleValue = strtold(inpBegin, &endPointer);
        WriteDoubleToMemory(argumentType->BitEncoding(), argumentType->TopAQSize(), argument->_pData, doubleValue);
    }
    break;
    case kEncoding_Array: {

        TypedArrayCoreRef* pArray = (TypedArrayCoreRef*)(argument->_pData);
        TypeRef elementType = (*pArray)->ElementType();
        EncodingEnum elementEncoding = elementType->BitEncoding();
        if (argumentType->Rank()==1 && (elementEncoding == kEncoding_Ascii || elementEncoding == kEncoding_Unicode)) {

            if (formatOptions->FormatChar == 's') {
                Boolean found = false;
                char* start = (char*)in.Begin();
                IntIndex stringStart = -1;
                IntIndex i = 0;
                for ( i=0; i< in.Length();i++) {
                    char c = *(start + i);
                    if (found && isspace(c)) {
                        i--;
                        break;
                    }
                    if (!found && !isspace(c)) {
                        found = true;
                        stringStart = i;
                    }
                }
                if (!found) {
                    return false;
                } else {
                    if (i==in.Length()){
                        // reach the end of the input
                        i--;
                    }
                    if (formatOptions->MinimumFieldWidth > 0 && i+1-stringStart > formatOptions->MinimumFieldWidth) {
                        i = stringStart + formatOptions->MinimumFieldWidth - 1;
                    }
                    endPointer = inpBegin + i+1;
                    (*pArray)->Replace1D(0, i+1-stringStart, in.Begin()+stringStart, true);

                }
            } else if (formatOptions->FormatChar == '[') {
                SubString* charSet = (SubString*) &(formatOptions->FmtSubString);
                charSet->AliasAssign(charSet->Begin()+1,charSet->End()-1);
                if (charSet->Length() == 0) {
                    return false;
                } else if (*((char *)charSet->Begin()) == '^'){
                    if (charSet->Length() == 1) {
                        return false;
                    } else {
                        Boolean found = false;
                        char* start = (char*)in.Begin();
                        IntIndex stringStart = -1;
                        IntIndex i = 0;
                        for ( i=0; i< in.Length();i++) {
                            Utf8Char c = *(start + i);
                            if (found && BelongtoCharSet(charSet, c)) {
                                i--;
                                break;
                            }
                            if (!found && !BelongtoCharSet(charSet, c)) {
                                found = true;
                                stringStart = i;
                            }
                        }
                        if (!found) {
                            return false;
                        } else {
                            if (i==in.Length()){
                            // reach the end of the input
                                i--;
                            }
                            if (formatOptions->MinimumFieldWidth > 0 && i+1-stringStart > formatOptions->MinimumFieldWidth) {
                                i = stringStart + formatOptions->MinimumFieldWidth - 1;
                            }
                            endPointer = inpBegin + i+1;
                            (*pArray)->Replace1D(0, i+1-stringStart, in.Begin()+stringStart, true);
                        }
                    }
                } else {
                    Boolean found = false;
                    char* start = (char*)in.Begin();
                    IntIndex stringStart = -1;
                    IntIndex i = 0;
                    for ( i=0; i< in.Length();i++) {
                        Utf8Char c = *(start + i);
                        if (found && !BelongtoCharSet(charSet, c)) {
                            i--;
                            break;
                        }
                        if (!found && BelongtoCharSet(charSet, c)) {
                            found = true;
                            stringStart = i;
                        }
                    }
                    if (!found) {
                        return false;
                    } else {
                        if (i==in.Length()){
                        // reach the end of the input
                            i--;
                        }
                        if (formatOptions->MinimumFieldWidth > 0 && i+1-stringStart > formatOptions->MinimumFieldWidth) {
                            i = stringStart + formatOptions->MinimumFieldWidth - 1;
                        }
                        endPointer = inpBegin + i+1;
                        (*pArray)->Replace1D(0, i+1-stringStart, in.Begin()+stringStart, true);
                    }
                }
            }
        } else {
            //doesn't support more complex array type
            return false;
        }
    }
    break;
    default:
        // doesn't support this kind of data type yet.
        return false;
    break;
    }
    if (endPointer == null || (endPointer == inpBegin)) {
        return false;
    }
    *endToken = (IntIndex)(endPointer-inpBegin);
    return true;
}
//---------------------------------------------------------------------------------------------
/*
 * The return value is the offset of the input string after the scan.
 * Several Special Scan rules:
 * */
Int32 FormatScan(SubString *input, SubString *format, StaticTypeAndData arguments[])
{
    // the rules for ScanString in Labview is a subset of the sscanf.
    // p will be treated as f;
    // binary should be processed.
    // should be very careful when try to parse a float with local decimal point

    IntIndex argumentIndex = 0;
    IntIndex filledItems = 0;
    char activeDecimalPoint = '.';
    TempStackCString tempFormat((Utf8Char*)"%", 1);
    Utf8Char c = 0;
    Utf8Char inputChar = 0;
    Boolean canScan = true;
    SubString f(format);
    while (canScan && input->Length()>0 && f.ReadRawChar(&c))
    {
        if (isspace(c)) {
            // eat all spaces
            const Utf8Char* begin = input->Begin();
            while (begin < input->End()) {
                if (isspace(*((char*)begin))) {
                    begin++;
                } else {
                    break;
                }
            }
            input->AliasAssign((Utf8Char*)begin, input->End());
        } else if (c == '%') {
            FormatOptions fOptions;
            ReadPercentFormatOptions(&f, &fOptions);
             // We should assign the local decimal point to DecimalSeparator.
            fOptions.DecimalSeparator = activeDecimalPoint;
            Boolean parseFinished = false;
            if (!fOptions.Valid || input->Length() <= 0) {
                parseFinished = true;
                canScan = false;
            }
            TempStackCString formatString;
            IntIndex endPointer;
            while (!parseFinished){
                parseFinished = true;
                switch (fOptions.FormatChar) {
                case 'b': case 'B':
                {
                    canScan = TypedScanString(input, &endPointer, &fOptions, &(arguments[argumentIndex]), &formatString);
                    if (canScan) {
                        filledItems++;
                        input->AliasAssign(input->Begin()+endPointer, input->End());
                    }
                    argumentIndex++;
                }
                break;
                case 'd':
                case 'o': case 'u':
                case 'x': case 'X':
                {
                    canScan = TypedScanString(input, &endPointer, &fOptions, &(arguments[argumentIndex]), &formatString);
                    if  (canScan) {
                        filledItems++;
                        input->AliasAssign(input->Begin()+endPointer, input->End());
                    }
                    argumentIndex++;
                }
                break;
                case 'g': case 'p':
                case 'e': case 'E':
                case 'f': case 'F':
                {
                    canScan = TypedScanString(input, &endPointer, &fOptions, &(arguments[argumentIndex]), &formatString);
                    if (canScan) {
                        filledItems++;
                        input->AliasAssign(input->Begin()+endPointer, input->End());
                    }
                    argumentIndex++;
                }
                break;
                case 's': case '[':
                {
                    canScan = TypedScanString(input, &endPointer, &fOptions, &(arguments[argumentIndex]), &formatString);
                    if (canScan) {
                        filledItems++;
                        input->AliasAssign(input->Begin()+endPointer, input->End());
                    }
                    argumentIndex++;
                }
                break;
                case '%': {    //%%
                    input->ReadRawChar(&inputChar);
                    if (inputChar == '%') {
                        input->AliasAssign(input->Begin()+1,input->End());
                        format->AliasAssign(format->Begin()+1, format->End());
                        } else{
                            canScan = false;
                        }
                }
                break;
                default:
                    canScan = false;
                break;
                }
             }
        } else {
            if (input->ReadRawChar(&inputChar)) {
                if (inputChar != c) {
                    canScan = false;
                    input->AliasAssign(input->Begin()-1, input->End());
                }
            } else {
                canScan = false;
            }
        }
    }
    return filledItems;
}
//-------------------------------------------------------------
/**
 *case 1 The format length is 0:
 *         In labview FormatValue function:
 *             print the value as %f no matter what the data type it is.
 *         In Format into String function:
 *            print the value with the proper format code according to the data type.
 *                 %f for float, %d for int. etc.
 *case 2 The format contains only non-foramt code character. e.g. "  asvasd ":
 *         In labview FormatValue function:
 *             append the format string to the output no value String is printed.
 *         In Format into String function:
 *             will get the too few specifier error!.
 *case 3 The first format code is not valid:
 *         In labview Format Value function:
 *             Parse the format code end stop at the invalid position. then print the value as in case 1.
 *             And append the remaining part of the format code.
 *         In labview Format into String function:
 *             throw the error. Report which argument the parsing has stopped at.
 *case 4 The format string contains more format code that needed. The needed format code is correct.
 *        In labview Format Value function:
 *            Only recognize the first format code and append the entire remaining format string to the printed value string.
 *        In labview Format Into String function:
 *            Whether the extra format codes is valid ot not, the function doesn't output anything, Just throw the error.
 *case 5 The format code and the type of the input value doesn't match
 *        In labview Format Value function:
 *            Use the Type conversion to convert the input value to the correct data type. Then print it.
 *        In labview Format intoString function:
 *        case 5.1 complex double as the numeric
 *            If the  data type of the input value is complex, the print value should print both the real part and imaginary part.
 *        case 5.2 string as numeric %d , %f
 *            Will not accept this case, throw the error.
 *case 6 The length speicfier
 *        In lbivew Format Value function:
 *            'h' 'l' is supported for %d and %f .. numeric type.
 *        In labview Format into String function:
 *            'h' is not supported.
 * */
VIREO_FUNCTION_SIGNATURE4(StringFormatValue, StringRef, StringRef, StaticType, void)
{
    StringRef output = _Param(0);
    StringRef formatString = _Param(1);

    SubString format = formatString->MakeSubStringAlias();
    StaticTypeAndData Value  = {_ParamPointer(2), _ParamPointer(3)};
    Utf8Char c = 0;
    Utf8Char defaultFormat[stringBufferSize];
    SubString remainingFormat;
    TempStackCString tempformat;

    if(format.Length() == 0) {
        DefaultFormatCode(1,&Value, &tempformat);
    } else {
        Boolean foundFirst = false;
        Utf8Char* index = NULL;
        while (format.ReadRawChar(&c))
        {
            index = (Utf8Char*)format.Begin();
            if (c == '%') {
                FormatOptions fOptions;
                ReadPercentFormatOptions(&format, &fOptions);
                if (!fOptions.Valid) {
                    remainingFormat.AliasAssign(format.Begin(), format.End());
                    format.AliasAssign(formatString->Begin(), index);
                    tempformat.Append(&format);
                    DefaultFormatCode(1,&Value, &tempformat);
                    break;
                } else if (fOptions.ConsumeArgument){
                    remainingFormat.AliasAssign(format.Begin(), format.End());
                    format.AliasAssign(formatString->Begin(), format.Begin());
                    tempformat.Append(&format);
                    break;
                }
            } else {
            }
        }
    }
    format.AliasAssign(tempformat.Begin(), tempformat.End());
    Format(&format, 1, &Value, output);
    output->AppendSubString(&remainingFormat);

    return _NextInstruction();
}
//------------------------------------------------------------
struct StringFormatStruct : public VarArgInstruction
{
    _ParamDef(StringRef, StringOut);
    _ParamDef(StringRef, StringFormat);
    _ParamImmediateDef(StaticTypeAndData, argument1[1]);
    NEXT_INSTRUCTION_METHODV()
};

VIREO_FUNCTION_SIGNATUREV(StringFormat, StringFormatStruct)
{
    Int32 count = (_ParamVarArgCount() -2)/2;
    StaticTypeAndData *arguments =  _ParamImmediate(argument1);
    SubString format = _Param(StringFormat)->MakeSubStringAlias();
    TempStackCString tempformat;
    if(format.Length() == 0) {
        DefaultFormatCode(count,arguments, &tempformat);
        format.AliasAssign(tempformat.Begin(), tempformat.End());
    }
    StringRef buffer = _Param(StringOut);
    Format(&format, count, arguments, buffer);
    return _NextInstruction();
}

/*
 * The scan function:
 *         case 1: length of input format code is 0
 *             use default behaviour
 *         case 2: invalid input format code
 *             scan value will use the default format code
 *             scan string will throw the error.
 *         case 3: The format code and the output value doesn't match.
 *            Parse the value according the format code. Then convert the parsed value to the output type. The default output type is double.
 *            The labview will automatically change the output value when your wire a input with the different input.
 *            e.g."10.85" %f -> int 11.0
 *                "10.85" %d -> int 10
 *                "10.85" %d ->double 10
 *            be careful, the conversion means different from the conversion in C++.
 *            It only reach the max value or min value.
 *            Be careful, complex double only sipport %f,
 *            if you use the %d, it only read the int.
 *
 * */
//------------------------------------------------------------
VIREO_FUNCTION_SIGNATURE5(StringScanValue, StringRef, StringRef, StringRef, StaticType, void)
{
    return _NextInstruction();
}
//------------------------------------------------------------
struct StringScanStruct : public VarArgInstruction
{
    _ParamDef(StringRef, StringInput);
    _ParamDef(StringRef, StringRemaining);
    _ParamDef(StringRef, StringFormat);
    _ParamDef(Int32, InitialPos);
    _ParamDef(Int32, OffsetPast);
    _ParamImmediateDef(StaticTypeAndData, argument1[1]);
    NEXT_INSTRUCTION_METHODV()
};

VIREO_FUNCTION_SIGNATUREV(StringScan, StringScanStruct)
{
    //Int32 count = (_ParamVarArgCount() -4)/2;
    StaticTypeAndData *arguments =  _ParamImmediate(argument1);
    SubString format = _Param(StringFormat)->MakeSubStringAlias();
    SubString input = _Param(StringInput)->MakeSubStringAlias();
    input.AliasAssign(input.Begin() + _Param(InitialPos), input.End());
    Int32 filled = FormatScan(&input, &format, arguments);
    _Param(OffsetPast) = _Param(StringInput)->Length() - input.Length();
    _Param(StringRemaining)->Resize1D(input.Length());
    TypeRef elementType = _Param(StringRemaining)->ElementType();
    elementType->CopyData(input.Begin(), _Param(StringRemaining)->Begin(), input.Length());
    return _NextInstruction();
}

DEFINE_VIREO_BEGIN(LabVIEW_String)
    DEFINE_VIREO_FUNCTION(StringFormatValue, "p(o(.String) i(.String) i(.StaticTypeAndData))")
    DEFINE_VIREO_FUNCTION(StringFormat, "p(i(.VarArgCount) o(.String) i(.String) i(.StaticTypeAndData))")
    DEFINE_VIREO_FUNCTION(StringScanValue, "p(i(.String) o(.String) i(.String) o(.StaticTypeAndData))")
    DEFINE_VIREO_FUNCTION(StringScan, "p(i(.VarArgCount) i(.String) o(.String) i(.String) i(.UInt32) o(.UInt32) o(.StaticTypeAndData))")
DEFINE_VIREO_END()

} // namespace Vireo