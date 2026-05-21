#pragma once

#include <QVector>

struct H264CabacContextModel
{
    int stateIndex = 0;
    int valueMps = 0;
};

class H264CabacContextModelSet
{
public:
    explicit H264CabacContextModelSet(int modelCount = 0);

    int size() const;
    bool isInitialized(int ctxIdx) const;
    H264CabacContextModel model(int ctxIdx) const;
    void setModel(int ctxIdx, const H264CabacContextModel &model);

private:
    QVector<H264CabacContextModel> m_models;
    QVector<bool> m_initialized;
};

class H264CabacContextModelInitializer
{
public:
    static H264CabacContextModel initializedContextModel(int m, int n, int sliceQpY);
    static H264CabacContextModelSet initializeSliceContexts(bool isIntraSlice,
                                                           int cabacInitIdc,
                                                           int sliceQpY,
                                                           int maxCtxIdx = 23);
};
