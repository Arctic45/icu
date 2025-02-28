// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ******************************************************************************
 * Copyright (C) 2003-2013, International Business Machines Corporation
 * and others. All Rights Reserved.
 ******************************************************************************
 *
 * File PERSNCAL.CPP
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   9/23/2003   mehran      posted to icu-design
 *   10/1/2012   roozbeh     Fixed algorithm and heavily refactored and rewrote
 *                           based on the implementation of Gregorian
 *****************************************************************************
 */

#include "persncal.h"

#if !UCONFIG_NO_FORMATTING

#include "umutex.h"
#include "gregoimp.h" // Math
#include <float.h>

static const int16_t kPersianNumDays[]
= {0,31,62,93,124,155,186,216,246,276,306,336}; // 0-based, for day-in-year
static const int8_t kPersianMonthLength[]
= {31,31,31,31,31,31,30,30,30,30,30,29}; // 0-based
static const int8_t kPersianLeapMonthLength[]
= {31,31,31,31,31,31,30,30,30,30,30,30}; // 0-based

static const int32_t kPersianCalendarLimits[UCAL_FIELD_COUNT][4] = {
    // Minimum  Greatest     Least   Maximum
    //           Minimum   Maximum
    {        0,        0,        0,        0}, // ERA
    { -5000000, -5000000,  5000000,  5000000}, // YEAR
    {        0,        0,       11,       11}, // MONTH
    {        1,        1,       52,       53}, // WEEK_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // WEEK_OF_MONTH
    {        1,       1,        29,       31}, // DAY_OF_MONTH
    {        1,       1,       365,      366}, // DAY_OF_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DAY_OF_WEEK
    {        1,       1,         5,        5}, // DAY_OF_WEEK_IN_MONTH
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // AM_PM
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // HOUR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // HOUR_OF_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MINUTE
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // SECOND
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECOND
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // ZONE_OFFSET
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DST_OFFSET
    { -5000000, -5000000,  5000000,  5000000}, // YEAR_WOY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // DOW_LOCAL
    { -5000000, -5000000,  5000000,  5000000}, // EXTENDED_YEAR
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // JULIAN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // MILLISECONDS_IN_DAY
    {/*N/A*/-1,/*N/A*/-1,/*N/A*/-1,/*N/A*/-1}, // IS_LEAP_MONTH
};

U_NAMESPACE_BEGIN

static const int32_t PERSIAN_EPOCH = 1948320;

// Implementation of the PersianCalendar class

//-------------------------------------------------------------------------
// Constructors...
//-------------------------------------------------------------------------

const char *PersianCalendar::getType() const { 
    return "persian";
}

PersianCalendar* PersianCalendar::clone() const {
    return new PersianCalendar(*this);
}

PersianCalendar::PersianCalendar(const Locale& aLocale, UErrorCode& success)
  :   Calendar(TimeZone::forLocaleOrDefault(aLocale), aLocale, success)
{
    setTimeInMillis(getNow(), success); // Call this again now that the vtable is set up properly.
}

PersianCalendar::PersianCalendar(const PersianCalendar& other) : Calendar(other) {
}

PersianCalendar::~PersianCalendar()
{
}

//-------------------------------------------------------------------------
// Minimum / Maximum access functions
//-------------------------------------------------------------------------


int32_t PersianCalendar::handleGetLimit(UCalendarDateFields field, ELimitType limitType) const {
    return kPersianCalendarLimits[field][limitType];
}

//-------------------------------------------------------------------------
// Assorted calculation utilities
//

/**
 * Determine whether a year is a leap year in the Persian calendar
 */
UBool PersianCalendar::isLeapYear(int32_t year)
{
    int32_t remainder;
    ClockMath::floorDivide(25 * year + 11, 33, &remainder);
    return (remainder < 8);
}
    
/**
 * Return the day # on which the given year starts.  Days are counted
 * from the Persian epoch, origin 0.
 */
int32_t PersianCalendar::yearStart(int32_t year) {
    return handleComputeMonthStart(year,0,false);
}
    
/**
 * Return the day # on which the given month starts.  Days are counted
 * from the Persian epoch, origin 0.
 *
 * @param year  The Persian year
 * @param year  The Persian month, 0-based
 */
int32_t PersianCalendar::monthStart(int32_t year, int32_t month) const {
    return handleComputeMonthStart(year,month,true);
}
    
//----------------------------------------------------------------------
// Calendar framework
//----------------------------------------------------------------------

/**
 * Return the length (in days) of the given month.
 *
 * @param year  The Persian year
 * @param year  The Persian month, 0-based
 */
int32_t PersianCalendar::handleGetMonthLength(int32_t extendedYear, int32_t month) const {
    // If the month is out of range, adjust it into range, and
    // modify the extended year value accordingly.
    if (month < 0 || month > 11) {
        extendedYear += ClockMath::floorDivide(month, 12, &month);
    }

    return isLeapYear(extendedYear) ? kPersianLeapMonthLength[month] : kPersianMonthLength[month];
}

/**
 * Return the number of days in the given Persian year
 */
int32_t PersianCalendar::handleGetYearLength(int32_t extendedYear) const {
    return isLeapYear(extendedYear) ? 366 : 365;
}
    
//-------------------------------------------------------------------------
// Functions for converting from field values to milliseconds....
//-------------------------------------------------------------------------

// Return JD of start of given month/year
int32_t PersianCalendar::handleComputeMonthStart(int32_t eyear, int32_t month, UBool /*useMonth*/) const {
    // If the month is out of range, adjust it into range, and
    // modify the extended year value accordingly.
    if (month < 0 || month > 11) {
        eyear += ClockMath::floorDivide(month, 12, &month);
    }

    int32_t julianDay = PERSIAN_EPOCH - 1 + 365 * (eyear - 1) + ClockMath::floorDivide(8 * eyear + 21, 33);

    if (month != 0) {
        julianDay += kPersianNumDays[month];
    }

    return julianDay;
}

//-------------------------------------------------------------------------
// Functions for converting from milliseconds to field values
//-------------------------------------------------------------------------

int32_t PersianCalendar::handleGetExtendedYear() {
    int32_t year;
    if (newerField(UCAL_EXTENDED_YEAR, UCAL_YEAR) == UCAL_EXTENDED_YEAR) {
        year = internalGet(UCAL_EXTENDED_YEAR, 1); // Default to year 1
    } else {
        year = internalGet(UCAL_YEAR, 1); // Default to year 1
    }
    return year;
}

/**
 * Override Calendar to compute several fields specific to the Persian
 * calendar system.  These are:
 *
 * <ul><li>ERA
 * <li>YEAR
 * <li>MONTH
 * <li>DAY_OF_MONTH
 * <li>DAY_OF_YEAR
 * <li>EXTENDED_YEAR</ul>
 * 
 * The DAY_OF_WEEK and DOW_LOCAL fields are already set when this
 * method is called.
 */
void PersianCalendar::handleComputeFields(int32_t julianDay, UErrorCode &/*status*/) {
    int32_t year, month, dayOfMonth, dayOfYear;

    int32_t daysSinceEpoch = julianDay - PERSIAN_EPOCH;
    year = 1 + (int32_t)ClockMath::floorDivide(33 * (int64_t)daysSinceEpoch + 3, (int64_t)12053);

    int32_t farvardin1 = 365 * (year - 1) + ClockMath::floorDivide(8 * year + 21, 33);
    dayOfYear = (daysSinceEpoch - farvardin1); // 0-based
    if (dayOfYear < 216) { // Compute 0-based month
        month = dayOfYear / 31;
    } else {
        month = (dayOfYear - 6) / 30;
    }
    dayOfMonth = dayOfYear - kPersianNumDays[month] + 1;
    ++dayOfYear; // Make it 1-based now

    internalSet(UCAL_ERA, 0);
    internalSet(UCAL_YEAR, year);
    internalSet(UCAL_EXTENDED_YEAR, year);
    internalSet(UCAL_MONTH, month);
    internalSet(UCAL_DAY_OF_MONTH, dayOfMonth);
    internalSet(UCAL_DAY_OF_YEAR, dayOfYear);
}    

// default century

static UDate           gSystemDefaultCenturyStart       = DBL_MIN;
static int32_t         gSystemDefaultCenturyStartYear   = -1;
static icu::UInitOnce  gSystemDefaultCenturyInit        {};

UBool PersianCalendar::haveDefaultCentury() const
{
    return true;
}

static void U_CALLCONV initializeSystemDefaultCentury() {
    // initialize systemDefaultCentury and systemDefaultCenturyYear based
    // on the current time.  They'll be set to 80 years before
    // the current time.
    UErrorCode status = U_ZERO_ERROR;
    PersianCalendar calendar(Locale("@calendar=persian"),status);
    if (U_SUCCESS(status))
    {
        calendar.setTime(Calendar::getNow(), status);
        calendar.add(UCAL_YEAR, -80, status);

        gSystemDefaultCenturyStart = calendar.getTime(status);
        gSystemDefaultCenturyStartYear = calendar.get(UCAL_YEAR, status);
    }
    // We have no recourse upon failure unless we want to propagate the failure
    // out.
}

UDate PersianCalendar::defaultCenturyStart() const {
    // lazy-evaluate systemDefaultCenturyStart
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStart;
}

int32_t PersianCalendar::defaultCenturyStartYear() const {
    // lazy-evaluate systemDefaultCenturyStartYear
    umtx_initOnce(gSystemDefaultCenturyInit, &initializeSystemDefaultCentury);
    return gSystemDefaultCenturyStartYear;
}

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PersianCalendar)

U_NAMESPACE_END

#endif

