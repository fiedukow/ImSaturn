// dibdoc.h : interface of the CDibDoc class
//
// This is a part of the Microsoft Foundation Classes C++ library.
// Copyright (C) 1992-1997 Microsoft Corporation
// All rights reserved.
//
// This source code is only intended as a supplement to the
// Microsoft Foundation Classes Reference and related
// electronic documentation provided with the library.
// See these sources for detailed information regarding the
// Microsoft Foundation Classes product.

#include "dibapi.h"
class CDibDoc : public CDocument
{
protected: // create from serialization only
	CDibDoc();
	DECLARE_DYNCREATE(CDibDoc)

// Attributes
public:
	HDIB GetHDIB() const
		{ return m_hDIB; }
	CPalette* GetDocPalette() const
		{ return m_palDIB; }
	CSize GetDocSize() const
		{ return m_sizeDoc; }

// Operations
public:
	static int m_size;
	void MagiczneCwiartki(CView *view);
	void DoLab2(CView *view);
	void BnWCalculate(CView * view, int green_value);
	void Field(CView* view, int green_value);
	void BoundingRect(RGBTRIPLE** rows, int width, int height, int* x1, int* y1, int* x2, int* y2, RGBTRIPLE color);
	void ConvertToBlockedImage(CView *view);
	void trace(LPCSTR msg);
	void DoNothing(CView *view);
	void ConvertToGrayImage(CView *view);
	void ReplaceHDIB(HDIB hDIB);
	void InitDIBData();

// Implementation
protected:
	virtual ~CDibDoc();

	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);

protected:
	HDIB m_hDIB;
	CPalette* m_palDIB;
	CSize m_sizeDoc;

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
protected:
	virtual BOOL    OnNewDocument();

// Generated message map functions
protected:
	//{{AFX_MSG(CDibDoc)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
private:
	CEditView* m_outputView;
};

/////////////////////////////////////////////////////////////////////////////
