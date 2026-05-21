#pragma once

#include <QString>
#include <QVector>

struct H264CabacSyntaxResult
{
    bool ok = false;
    int value = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMbTypeResult
{
    bool ok = false;
    bool complete = false;
    bool needsSubMacroblockTypes = false;
    int mbType = -1;
    int prefixBin = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacSubMbTypeResult
{
    bool ok = false;
    bool complete = false;
    int subMbType = -1;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacSubMbTypesResult
{
    bool ok = false;
    bool complete = false;
    QVector<int> subMbTypes;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacRefIdxResult
{
    bool ok = false;
    bool present = false;
    int refIdx = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacRefIdxListResult
{
    bool ok = false;
    bool present = false;
    QVector<int> refIdx;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMvdResult
{
    bool ok = false;
    bool complete = false;
    int value = 0;
    int ctxIdx = -1;
    QString diagnosticCode;
    QString diagnosticMessage;
};

struct H264CabacMvdPair
{
    int x = 0;
    int y = 0;
};

struct H264CabacMvdListResult
{
    bool ok = false;
    bool complete = false;
    QVector<H264CabacMvdPair> mvd;
    QString diagnosticCode;
    QString diagnosticMessage;
};
