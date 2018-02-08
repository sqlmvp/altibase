/** 
 *  Copyright (c) 1999~2017, Altibase Corp. and/or its affiliates. All rights reserved.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 3,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 

/***********************************************************************
 * $Id: mtddl.l 74259 2016-01-26 01:25:03Z abel.chun $
 **********************************************************************/

/***********************************************************************
 * ATTENTION : 
 ***********************************************************************
 * BUG-14591
 * This file must be generated by flex 2.5.33 with -R option in V880
 * If you update mtddl.l, please follow this procedure.
 1) make genclean
 2) make genflex
 3) commit mtddl.l
 4) commit mtddl.cpp
 5) commit ../include/mtddlLexer.h
 **********************************************************************/
%option reentrant
%option 8bit
%option caseless
%option never-interactive
%option noreject
%option noyymore
%option noyywrap
%option nostdinit
%option prefix="mtddl"
%option header-file="mtddlLexer.h" 

%{
#include <idl.h>
#include <mte.h>
#include <mtc.h>
#include <mtd.h>
#include <mtl.h>
#include <mtdTypes.h>

#undef ECHO

%}

%%

AM          { 
                if( idlOS::strMatch( yyget_text(yyscanner), 2, "AM", 2 ) == 0 )
                {
                    return MTD_DATE_FORMAT_AM_U;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 2, "Am", 2 ) == 0 )
                {
                    return MTD_DATE_FORMAT_AM_UL;
                }
                else if( ( idlOS::strMatch( yyget_text(yyscanner), 2, "aM", 2 ) == 0 ) ||
                         ( idlOS::strMatch( yyget_text(yyscanner), 2, "am", 2 ) == 0 ) )
                {
                    return MTD_DATE_FORMAT_AM_L;
                }
            }
SCC         {
                return MTD_DATE_FORMAT_SCC;
            }
CC          { 
                return MTD_DATE_FORMAT_CC;
            }
DAY         { 
                if( idlOS::strMatch( yyget_text(yyscanner), 3, "DAY", 3 ) == 0 )
                {
                    return MTD_DATE_FORMAT_DAY_U;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 3, "Day", 3 ) == 0 )
                {
                    return MTD_DATE_FORMAT_DAY_UL;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 3, "day", 3 ) == 0 )
                {
                    return MTD_DATE_FORMAT_DAY_L;
                }
            }
DDD         { 
                return MTD_DATE_FORMAT_DDD;
            }
DD          { 
                return MTD_DATE_FORMAT_DD;
            }
DY          { 
                if( idlOS::strMatch( yyget_text(yyscanner), 2, "DY", 2 ) == 0 )
                {
                    return MTD_DATE_FORMAT_DY_U;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 2, "Dy", 2 ) == 0 )
                {
                    return MTD_DATE_FORMAT_DY_UL;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 2, "dy", 2 ) == 0 )
                {
                    return MTD_DATE_FORMAT_DY_L;
                }
            }
D           { 
                return MTD_DATE_FORMAT_D;
            }
FF          {
                return MTD_DATE_FORMAT_FF;
            }
FM          {
                return MTD_DATE_FORMAT_FM;
            }
FF1         {
                return MTD_DATE_FORMAT_FF1;
            }
FF2         { 
                return MTD_DATE_FORMAT_FF2;
            }
FF3         { 
                return MTD_DATE_FORMAT_FF3;
            }
FF4         { 
                return MTD_DATE_FORMAT_FF4;
            }
FF5         { 
                return MTD_DATE_FORMAT_FF5;
            }
FF6         { 
                return MTD_DATE_FORMAT_FF6;
            }
HH          { 
                return MTD_DATE_FORMAT_HH;
            }
HH12        { 
                return MTD_DATE_FORMAT_HH12;
            }
HH24        { 
                return MTD_DATE_FORMAT_HH24;
            }
MI          { 
                return MTD_DATE_FORMAT_MI;
            }
MM          { 
                return MTD_DATE_FORMAT_MM;
            }
MON         { 
                if( idlOS::strMatch( yyget_text(yyscanner), 3, "MON", 3 ) == 0 )
                {
                    return MTD_DATE_FORMAT_MON_U;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 3, "Mon", 3 ) == 0 )
                {
                    return MTD_DATE_FORMAT_MON_UL;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 3, "mon", 3 ) == 0 )
                {
                    return MTD_DATE_FORMAT_MON_L;
                }
            }
MONTH       { 
                if( idlOS::strMatch( yyget_text(yyscanner), 5, "MONTH", 5 ) == 0 )
                {
                    return MTD_DATE_FORMAT_MONTH_U;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 5, "Month", 5 ) == 0 )
                {
                    return MTD_DATE_FORMAT_MONTH_UL;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 5, "month", 5 ) == 0 )
                {
                    return MTD_DATE_FORMAT_MONTH_L;
                }
            }
PM          { 
                if( idlOS::strMatch( yyget_text(yyscanner), 2, "PM", 2 ) == 0 )
                {
                    return MTD_DATE_FORMAT_PM_U;
                }
                else if( idlOS::strMatch( yyget_text(yyscanner), 2, "Pm", 2 ) == 0 )
                {
                    return MTD_DATE_FORMAT_PM_UL;
                }
                else if( ( idlOS::strMatch( yyget_text(yyscanner), 2, "pM", 2 ) == 0 ) ||
                         ( idlOS::strMatch( yyget_text(yyscanner), 2, "pm", 2 ) == 0 ) )
                {
                    return MTD_DATE_FORMAT_PM_L;
                }
            }
Q           { 
                return MTD_DATE_FORMAT_Q;
            }
RM          { 
                if( ( idlOS::strMatch( yyget_text(yyscanner), 2, "RM", 2 ) == 0 ) ||
                    ( idlOS::strMatch( yyget_text(yyscanner), 2, "Rm", 2 ) == 0 ) )
                {
                    return MTD_DATE_FORMAT_RM_U;
                }
                else if( ( idlOS::strMatch( yyget_text(yyscanner), 2, "rM", 2 ) == 0 ) ||
                         ( idlOS::strMatch( yyget_text(yyscanner), 2, "rm", 2 ) == 0 ) )
                {
                    return MTD_DATE_FORMAT_RM_L;
                }
            }
RRRR        { 
                return MTD_DATE_FORMAT_RRRR;
            }
RR          { 
                return MTD_DATE_FORMAT_RR;
            }
SSSSSSSS    { 
                return MTD_DATE_FORMAT_SSSSSSSS;
            }
SSSSSS      { 
                return MTD_DATE_FORMAT_SSSSSS;
            }
SSSSS       { 
                return MTD_DATE_FORMAT_SSSSS;
            }
SS          { 
                return MTD_DATE_FORMAT_SS;
            }
IW          {
                return MTD_DATE_FORMAT_IW;
            }
WW2         {
                return MTD_DATE_FORMAT_WW2;
            }
WW          { 
                return MTD_DATE_FORMAT_WW;
            }
W           { 
                return MTD_DATE_FORMAT_W;
            }
SYYYY       {
                return MTD_DATE_FORMAT_SYYYY;
            }
Y,YYY       { 
                return MTD_DATE_FORMAT_YCYYY;
            }
YYYY        { 
                return MTD_DATE_FORMAT_YYYY;
            }
YYY         { 
                return MTD_DATE_FORMAT_YYY;
            }
YY          { 
                return MTD_DATE_FORMAT_YY;
            }
Y           { 
                return MTD_DATE_FORMAT_Y;
            }

\"[^\"]*\"  {
                return MTD_DATE_FORMAT_DOUBLE_QUOTE_STRING;
            }

[-/,.:;\'\n\r\t !#$%()*+\\<=>?@\[\]\^_`{}|~]+ {
                return MTD_DATE_FORMAT_SEPARATOR;
            }

.           { 
                return MTD_DATE_FORMAT_NONE;
            }
%%