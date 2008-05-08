#ifndef CINPUTFRAME_H
#define CINPUTFRAME_H

class CInputFrame : public CWindow
{
public:
    static LRESULT CALLBACK InputWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    CInputFrame();
    bool Create(CWindow *pParentWindow, int x, int y, int cx, int cy);
    virtual ~CInputFrame();

    // Handlers.
    //
    LRESULT OnCreate(CREATESTRUCT *pcs);
};

#endif // CINPUTFRAME_H