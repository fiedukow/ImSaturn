// dibdoc.cpp : implementation of the CDibDoc class
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

#include "stdafx.h"
#include "diblook.h"
#include <limits.h>
#include <algorithm>
#include <sstream>
#define M_PI 3.14159265358979323846
#include <cmath>

#include "dibdoc.h"
#include "dibview.h"
#include "outDoc.h"
#include "afxext.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CDibDoc

IMPLEMENT_DYNCREATE(CDibDoc, CDocument)

BEGIN_MESSAGE_MAP(CDibDoc, CDocument)
	//{{AFX_MSG_MAP(CDibDoc)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDibDoc construction/destruction

int CDibDoc::m_size = 8;

CDibDoc::CDibDoc()
{
	m_hDIB = NULL;
	m_palDIB = NULL;
	m_sizeDoc = CSize(1,1);     // dummy value to make CScrollView happy
	m_outputView = NULL ;
}

CDibDoc::~CDibDoc()
{
	if (m_hDIB != NULL)
	{
		::GlobalFree((HGLOBAL) m_hDIB);
	}
	if (m_palDIB != NULL)
	{
		delete m_palDIB;
	}
}

BOOL CDibDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;
	return TRUE;
}

void CDibDoc::InitDIBData()
{
	if (m_palDIB != NULL)
	{
		delete m_palDIB;
		m_palDIB = NULL;
	}
	if (m_hDIB == NULL)
	{
		return;
	}
	// Set up document size
	LPSTR lpDIB = (LPSTR) ::GlobalLock((HGLOBAL) m_hDIB);
	if (::DIBWidth(lpDIB) > INT_MAX ||::DIBHeight(lpDIB) > INT_MAX)
	{
		::GlobalUnlock((HGLOBAL) m_hDIB);
		::GlobalFree((HGLOBAL) m_hDIB);
		m_hDIB = NULL;
		CString strMsg;
		strMsg.LoadString(IDS_DIB_TOO_BIG);
		MessageBox(NULL, strMsg, NULL, MB_ICONINFORMATION | MB_OK);
		return;
	}
	m_sizeDoc = CSize((int) ::DIBWidth(lpDIB), (int) ::DIBHeight(lpDIB));
	::GlobalUnlock((HGLOBAL) m_hDIB);
	// Create copy of palette
	m_palDIB = new CPalette;
	if (m_palDIB == NULL)
	{
		// we must be really low on memory
		::GlobalFree((HGLOBAL) m_hDIB);
		m_hDIB = NULL;
		return;
	}
	if (::CreateDIBPalette(m_hDIB, m_palDIB) == NULL)
	{
		// DIB may not have a palette
		delete m_palDIB;
		m_palDIB = NULL;
		return;
	}
}


BOOL CDibDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	CFile file;
	CFileException fe;
	if (!file.Open(lpszPathName, CFile::modeRead | CFile::shareDenyWrite, &fe))
	{
		ReportSaveLoadException(lpszPathName, &fe,
			FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
		return FALSE;
	}

	DeleteContents();
	BeginWaitCursor();

	// replace calls to Serialize with ReadDIBFile function
	TRY
	{
		m_hDIB = ::ReadDIBFile(file);
	}
	CATCH (CFileException, eLoad)
	{
		file.Abort(); // will not throw an exception
		EndWaitCursor();
		ReportSaveLoadException(lpszPathName, eLoad,
			FALSE, AFX_IDP_FAILED_TO_OPEN_DOC);
		m_hDIB = NULL;
		return FALSE;
	}
	END_CATCH

	InitDIBData();
	EndWaitCursor();

	if (m_hDIB == NULL)
	{
		// may not be DIB format
		CString strMsg;
		strMsg.LoadString(IDS_CANNOT_LOAD_DIB);
		MessageBox(NULL, strMsg, NULL, MB_ICONINFORMATION | MB_OK);
		return FALSE;
	}
	SetPathName(lpszPathName);
	SetModifiedFlag(FALSE);     // start off with unmodified
	return TRUE;
}


BOOL CDibDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	CFile file;
	CFileException fe;

	if (!file.Open(lpszPathName, CFile::modeCreate |
	  CFile::modeReadWrite | CFile::shareExclusive, &fe))
	{
		ReportSaveLoadException(lpszPathName, &fe,
			TRUE, AFX_IDP_INVALID_FILENAME);
		return FALSE;
	}

	// replace calls to Serialize with SaveDIB function
	BOOL bSuccess = FALSE;
	TRY
	{
		BeginWaitCursor();
		bSuccess = ::SaveDIB(m_hDIB, file);
		file.Close();
	}
	CATCH (CException, eSave)
	{
		file.Abort(); // will not throw an exception
		EndWaitCursor();
		ReportSaveLoadException(lpszPathName, eSave,
			TRUE, AFX_IDP_FAILED_TO_SAVE_DOC);
		return FALSE;
	}
	END_CATCH

	EndWaitCursor();
	SetModifiedFlag(FALSE);     // back to unmodified

	if (!bSuccess)
	{
		// may be other-style DIB (load supported but not save)
		//  or other problem in SaveDIB
		CString strMsg;
		strMsg.LoadString(IDS_CANNOT_SAVE_DIB);
		MessageBox(NULL, strMsg, NULL, MB_ICONINFORMATION | MB_OK);
	}

	return bSuccess;
}

void CDibDoc::ReplaceHDIB(HDIB hDIB)
{
	if (m_hDIB != NULL)
	{
		::GlobalFree((HGLOBAL) m_hDIB);
	}
	m_hDIB = hDIB;
}

/////////////////////////////////////////////////////////////////////////////
// CDibDoc diagnostics

#ifdef _DEBUG
void CDibDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CDibDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}

#endif //_DEBUG

/////////////////////////////////////////////////////////////////////////////
// CDibDoc commands

void CDibDoc::ConvertToGrayImage(CView *view)
{
    trace("start ConvertToGrayImage\r\n"); 

	RGBTRIPLE* rows[500] ;
	int width, height ;
	if( GetDIBRowsRGB(m_hDIB,rows,&width,&height)) {
		for( int x=0;x<width;x++)
			for( int y =0;y<height;y++) {
				int gray = rows[y][x].rgbtBlue+rows[y][x].rgbtRed+rows[y][x].rgbtGreen ;
				gray /= 3 ;
				rows[y][x].rgbtRed = gray ;
				rows[y][x].rgbtGreen = gray ;
				rows[y][x].rgbtBlue = gray;
			}
		FreeDIBRows(m_hDIB);
	}
	UpdateAllViews(NULL);

    trace("stop ConvertToGrayImage\r\n"); 
}


void CDibDoc::DoNothing(CView *view)
{
}

void CDibDoc::trace(LPCSTR msg)
{
	COutputDoc* pOut ;
	if( m_outputView == NULL ) {
		pOut = (COutputDoc*)((CDibLookApp*)AfxGetApp())->m_outputTemplate->OpenDocumentFile(NULL);	
		pOut->SetModifiedFlag(TRUE);
		pOut->UpdateAllViews(NULL);
		POSITION pos = pOut->GetFirstViewPosition();
		CView* pFirstView = pOut->GetNextView( pos );
		m_outputView = (CEditView *)pFirstView;
	} else
		pOut = (COutputDoc*)m_outputView->GetDocument();
    m_outputView->GetEditCtrl().ReplaceSel(msg); 
	pOut->SetModifiedFlag(FALSE);
}

int cut(int x) {
	return min(max(x, 0), 255);
}
int gray(tagRGBTRIPLE& px) {
	return (px.rgbtBlue+px.rgbtRed+px.rgbtGreen)/3;
}

void CDibDoc::MagiczneCwiartki(CView* view) {	
	RGBTRIPLE* rows[500] ;
	int width, height ;
	if( GetDIBRowsRGB(m_hDIB,rows,&width,&height)) {
		// CWIARTKA 2
		for (int x = 0; x < width/2; ++x) {
			for (int y = 0; y < height/2; ++y) {
				rows[y][x].rgbtRed = cut(m_size + rows[y][x].rgbtRed);
				rows[y][x].rgbtGreen = cut(m_size + rows[y][x].rgbtGreen);
				rows[y][x].rgbtBlue = cut(m_size + rows[y][x].rgbtBlue);
			}
		}

		// CWIARTKA 3
		for (int x = width/2; x < width; ++x) {
			for (int y = height/2; y < height; ++y) {
				int gr = gray(rows[y][x]);
				int red = rows[y][x].rgbtRed;
				rows[y][x].rgbtRed = cut(gr - red);
				rows[y][x].rgbtGreen = cut(gr - red);
				rows[y][x].rgbtBlue = cut(gr - red);
			}
		}

		// CWIARTKA 4
		for (int x = width/2; x < width; ++x) {
			for (int y = 0; y < height/2; ++y) {
				int gr = gray(rows[y][x]);
				rows[y][x].rgbtRed = cut(rows[y][x].rgbtRed * (((double)m_size)/100.0 + 1));
				rows[y][x].rgbtGreen = cut(rows[y][x].rgbtGreen * (((double)m_size)/100.0 + 1));
				rows[y][x].rgbtBlue = cut(rows[y][x].rgbtBlue * (((double)m_size)/100.0 + 1));
			}
		}

		// HISTOGRAM		
		const int MAX_PX = 256;
		const int PARTS = 8;
		int histogram[PARTS];
		for (int i = 0; i < PARTS; ++i)
			histogram[i] = 0;
		for (int x = 0; x < width; ++x) {
			for (int y = 0; y < height; ++y) {
				int gr = gray(rows[y][x]);
				for (int i = 0; i < PARTS; ++i) {
					if (gr < (MAX_PX/PARTS)*(i+1)) {
						histogram[i]++;
						break;
					}
				}
			}
		}

		std::stringstream ss;
		int sum = 0;
		for (int i = 0; i < PARTS; ++i) {
			sum += histogram[i];
			ss << "H[" << i << "] = " << histogram[i] << "; ";
		}
		ss << "; SUMA: " << sum << ";\r\n";
		trace(ss.str().c_str());

		FreeDIBRows(m_hDIB);
	}
	UpdateAllViews(NULL);
}

#define WINDOW_SIZE 5

bool rgb_triple_gray_compare(RGBTRIPLE f, RGBTRIPLE s) {
	return gray(f) < gray(s);
}

void CDibDoc::DoLab2(CView* view) {
	// CPOO - modyfikuj tutaj
	RGBTRIPLE* rows_origin[500];
	RGBTRIPLE rows[500][500];
	int width, height ;

	if (!GetDIBRowsRGB(m_hDIB,rows_origin,&width,&height))
		return;

	for (int x = (WINDOW_SIZE/2); x < (width - (WINDOW_SIZE/2)); ++x)
	for (int y = (WINDOW_SIZE/2); y < (height - (WINDOW_SIZE/2)); ++y) {
		RGBTRIPLE window[WINDOW_SIZE*WINDOW_SIZE];
		for (int i = x - WINDOW_SIZE/2, ii = 0; i < x + WINDOW_SIZE/2 + 1; ++i, ++ii)
		for (int j = y - WINDOW_SIZE/2, jj = 0; j < y + WINDOW_SIZE/2 + 1; ++j, ++jj)
			window[ii*WINDOW_SIZE+jj] = rows_origin[j][i];	

		std::sort(window, window + WINDOW_SIZE*WINDOW_SIZE, rgb_triple_gray_compare);
		rows[y][x] = window[m_size];
	}

	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
		rows_origin[y][x] = rows[y][x];

	FreeDIBRows(m_hDIB);
	UpdateAllViews(NULL);
}

void CDibDoc::BoundingRect(RGBTRIPLE** rows, int width, int height, int* x1, int* y1, int* x2, int* y2, int green_value) {
	*x2 = *y2 = 0;
	*x1 = width - 1;
	*y1 = height - 1;

	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
	{
		if (rows[y][x].rgbtGreen == green_value) {
			if (x < *x1) *x1 = x;
			if (x > *x2) *x2 = x;
			if (y < *y1) *y1 = y;
			if (y > *y2) *y2 = y;
		}
	}
}

double mpq(RGBTRIPLE** rows, int x1, int y1, int x2, int y2, int green_value, double p, double q) {
	double result = 0.0;
	for (int x = x1; x <= x2; ++x)
	for (int y = y1; y <= y2; ++y)
		result += pow((double)x, p)*pow((double)y, q)*((rows[y][x].rgbtGreen == green_value) ? 1.0 : 0.0);
	return result;
}

double Mpq(RGBTRIPLE** rows, int x1, int y1, int x2, int y2, double ax, double ay, int green_value, double p, double q) {
	double result = 0.0;
	for (int x = x1; x <= x2; ++x)
	for (int y = y1; y <= y2; ++y)
		result += pow((double)x - ax, p)*pow((double)y - ay, q)*((rows[y][x].rgbtGreen == green_value) ? 1.0 : 0.0);
	return result;
}

void CDibDoc::BnWCalculate(CView * view, int green_value) {
	std::stringstream ss;
	ss << "Green value: " << green_value << "\r\n";
	trace(ss.str().c_str());
	RGBTRIPLE* rows[1000];
	int width, height;
	if (!GetDIBRowsRGB(m_hDIB,rows,&width,&height))
		return;
	int x1, y1, x2, y2;
	BoundingRect(rows, width, height, &x1, &y1, &x2, &y2, green_value);
	ss.str(""); ss.clear();
	ss << "Bounding rectangle: (" << x1 << ", " << y1 << ") x (" << x2 << ", " << y2 << ");" << "\r\n";
	trace(ss.str().c_str());
	const RGBTRIPLE red = {0, green_value, 255};
	const RGBTRIPLE blue = {255, green_value, 0};

	// POLE
	int s = 0;
	for (int x = x1; x <= x2; ++x)
	for (int y = y1; y <= y2; ++y)
		if (rows[y][x].rgbtGreen == green_value)
			++s;
	ss.str(""); ss.clear();
	ss << "Pole S = " << s << "\r\n";
	trace(ss.str().c_str());
	
	// OBWOD
	int l = 0;
	for (int x = x1; x <= x2; ++x)
	for (int y = y1; y <= y2; ++y)
		if (rows[y][x].rgbtGreen == green_value &&
			(rows[max(0, y)][max(0, x - 1)].rgbtGreen != green_value ||
			rows[min(height, y)][min(width, x + 1)].rgbtGreen != green_value ||
			rows[max(0, y - 1)][max(0, x)].rgbtGreen != green_value ||
			rows[min(height, y + 1)][min(width, x)].rgbtGreen != green_value)) {
			++l;
			rows[y][x] = red;
		}
	ss.str(""); ss.clear();
	ss << "Obwod L = " << l << "\r\n";
	trace(ss.str().c_str());

	// W3
	double w3 = (l / (2 * sqrt(M_PI*s))) - 1;
	ss.str(""); ss.clear();
	ss << "W3 = " << w3 << "\r\n";
	trace(ss.str().c_str());

	// M3
#define m(p,q) mpq(rows,x1,y1,x2,y2,green_value,(p),(q))
#define M(p,q) Mpq(rows,x1,y1,x2,y2,ax,ay,green_value,(p),(q))
#define N(p,q) M((p),(q))/pow(m(0,0), (((p)+(q))/2) + 1)
	double sm = m(0,0);
	double ay = m(0,1)/sm;
	double ax = m(1,0)/sm;	
	ss.str(""); ss.clear();
	ss << "Srodek Ciezkosci = (" << ax << ", " << ay << "); Pole z mpq = " << sm << "\r\n";
	trace(ss.str().c_str());

	double M3 = pow((N(3.0,0.0) - 3.0*N(1.0,2.0)), 2.0) + pow((3.0*N(2.0,1.0) - N(0.0,3.0)), 2.0);
	double M7 = N(2.0,0.0)*N(0.0,2.0) - pow(N(1.0,1.0),2.0);
	ss.str(""); ss.clear();
	ss << "M3 = " << M3 << "; M7 = " << M7 << ";\r\n";
	trace(ss.str().c_str());

	double bbmx = ((x2-x1)/2) + x1;
	double bbmy = ((y2-y1)/2) + y1;
	ss.str(""); ss.clear();
	ss << "Srodek bounding boxa = (" << bbmx << ", " << bbmy << ");\r\n";
	trace(ss.str().c_str());

	double rd = atan((bbmx-ax)/(bbmy-ay));
	ss.str(""); ss.clear();
	ss << "Kat  = " << (rd/(2*M_PI))*360 << "\r\n";
	trace(ss.str().c_str());

	rows[min(max(0,(int)ay),height-1)][min(max(0,(int)ax),width-1)] = blue;
	rows[min(max(0,(int)bbmy),height-1)][min(max(0,(int)bbmx),width-1)] = red;

	ss.str(""); ss.clear();
	ss << "===================================================================\r\n\r\n";
	trace(ss.str().c_str());
}

void CDibDoc::ConvertToBlockedImage(CView * view)
{	
	BnWCalculate(view, 180);
	BnWCalculate(view, 135);
	BnWCalculate(view, 90);
	BnWCalculate(view, 45);	
	BnWCalculate(view, 0);
}
