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
#include <cassert>
#include <set>
#include <limits>
#include <iomanip>

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

bool isLogoPart(RGBTRIPLE px) {
	return gray(px) > 220;
}

bool operator==(const RGBTRIPLE& first,  const RGBTRIPLE& second) {
	return first.rgbtBlue  == second.rgbtBlue  &&
		   first.rgbtGreen == second.rgbtGreen &&
		   first.rgbtRed   == second.rgbtRed;
}

bool operator!=(const RGBTRIPLE& first,  const RGBTRIPLE& second) {
	return !(first == second);
}

void CDibDoc::BoundingRect(RGBTRIPLE** rows, int width, int height, int* x1, int* y1, int* x2, int* y2, RGBTRIPLE color) {
	*x2 = *y2 = 0;
	*x1 = width - 1;
	*y1 = height - 1;

	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
	{
		if (rows[y][x] == color) {
			if (x < *x1) *x1 = x;
			if (x > *x2) *x2 = x;
			if (y < *y1) *y1 = y;
			if (y > *y2) *y2 = y;
		}
	}
}

double mpq(RGBTRIPLE** rows, int x1, int y1, int x2, int y2, RGBTRIPLE color, double p, double q) {
	double result = 0.0;
	for (int x = x1; x <= x2; ++x)
	for (int y = y1; y <= y2; ++y)
		result += pow((double)x, p)*pow((double)y, q)*((rows[y][x] == color) ? 1.0 : 0.0);
	return result;
}

double Mpq(RGBTRIPLE** rows, int x1, int y1, int x2, int y2, double ax, double ay, RGBTRIPLE color, double p, double q) {
	double result = 0.0;
	for (int x = x1; x <= x2; ++x)
	for (int y = y1; y <= y2; ++y)
		result += pow((double)x - ax, p)*pow((double)y - ay, q)*((rows[y][x] == color) ? 1.0 : 0.0);
	return result;
}

const RGBTRIPLE SEGMENTS[] = { {  0,   0, 255}, {255,   0,   0}, {  0, 255,   0},
  					           {255, 255,   0}, {  0, 255, 255}, {255,   0, 255} };
const RGBTRIPLE WHITE = { 255, 255, 255 };
const RGBTRIPLE BLACK = { 0, 0, 0 };

int segment_counter = 0;

RGBTRIPLE getNextSegmentColor(RGBTRIPLE& segment_color) {
	segment_color.rgbtRed++;
	if (segment_color.rgbtRed == 0) {
		segment_color.rgbtGreen++;
		if (segment_color.rgbtGreen == 0)
			segment_color.rgbtBlue++;
	}
	//FIXME OVERFLOW... LOL
	segment_counter++;
	return segment_color;
}

void ToBnW(RGBTRIPLE** rows, int width, int height) {
	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
	{
		if (isLogoPart(rows[y][x]))
			rows[y][x] = WHITE;
		else
			rows[y][x] = BLACK;
	}
}

void Recolor(RGBTRIPLE** rows, int x_b, int y_b, int width, int height, RGBTRIPLE color/* int level = 0, bool above = false*/) {
	std::set<std::pair<int, int>> todo;
	todo.insert(std::make_pair(x_b, y_b));
	while (!todo.empty()) {
		int x = (*todo.begin()).first;
		int y = (*todo.begin()).second;
		todo.erase(todo.begin());
		rows[y][x] = color;
		if (y > 0) {
			RGBTRIPLE top = rows[y - 1][x];
			if (top != color && top != BLACK)
				todo.insert(std::make_pair(x, y - 1));
		}
		if (x > 0) {
			RGBTRIPLE left = rows[y][x - 1];
			if (left != color && left != BLACK)
				todo.insert(std::make_pair(x - 1, y));
		}
		if (y < (height - 1)) {
			RGBTRIPLE bottom = rows[y + 1][x];
			if (bottom != color && bottom != BLACK)
				todo.insert(std::make_pair(x, y + 1));
		}
		if (x < (width - 1)) {
			RGBTRIPLE right = rows[y][x + 1];			
			if (right != color && right != BLACK)
				todo.insert(std::make_pair(x + 1, y));
		}
	}
}

int SegmentsMarker(RGBTRIPLE** rows, int width, int height) {	
	int segments = 0;
	RGBTRIPLE segment_color = { 0, 0, 0 };
	for (int x = 0, y = 0; y < height; ++y)
		rows[y][x] = BLACK;
	for (int x = 0, y = 0; x < width; ++x)
		rows[y][x] = BLACK;

	for (int x = 1; x < width; ++x)
	for (int y = 1; y < height; ++y) {
		const RGBTRIPLE this_color = rows[y][x];
		if (this_color == BLACK)
			continue;
		
		RGBTRIPLE top = rows[y-1][x];
		RGBTRIPLE left = rows[y][x-1];
		if (top != BLACK && left != BLACK &&
			top != left) {
			rows[y][x] = top;
			Recolor(rows, x-1, y, width, height, top);
		} else if (top != BLACK) {
			rows[y][x] = top;
		} else if (left != BLACK) {
			rows[y][x] = left;
		} else {
			rows[y][x] = getNextSegmentColor(segment_color);
			segments++;
		}
		assert(rows[y][x] == top || top == BLACK);
	}
	return segments;
}

void RecolorSegment(RGBTRIPLE** rows, int width, int height, RGBTRIPLE from, RGBTRIPLE to) {
	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y) {
		if (rows[y][x] == from)
			rows[y][x] = to;
	}
}

#define m(p,q) mpq(rows,x1,y1,x2,y2,segment_color,(p),(q))
#define M(p,q) Mpq(rows,x1,y1,x2,y2,ax,ay,segment_color,(p),(q))
#define N(p,q) M((p),(q))/pow(m(0,0), (((p)+(q))/2) + 1)

void CDibDoc::BnWCalculate(CView * view, int green_value) {
	std::stringstream ss;

	RGBTRIPLE* rows[1000];
	int width, height;
	if (!GetDIBRowsRGB(m_hDIB,rows,&width,&height))
		return;
	ToBnW(rows, width, height);
	int segments_found = SegmentsMarker(rows, width, height);
	RGBTRIPLE segment_color = { 0, 0, 0 };
	int big = 0;
	for (int i = 0; i < segments_found; ++i) {
		getNextSegmentColor(segment_color);
		int x1, y1, x2, y2;
		BoundingRect(rows, width, height, &x1, &y1, &x2, &y2, segment_color);
	 	double sm = m(0,0);
	 	double ay = m(0,1)/sm;
	 	double ax = m(1,0)/sm;	
		if (sm > 200) {
	 		ss.str(""); ss.clear();			
	 		ss << "Srodek Ciezkosci = (" << ax << ", " << ay << "); Pole z mpq = " << sm << "\r\n";
	 		trace(ss.str().c_str());
			double M1 = N(2.0, 0.0) + N(0.0, 2.0);
			double M2 = pow(N(2.0, 0.0) - N(0.0, 2.0), 2) + 4 * pow(N(1.0, 1.0), 2);
			double M3 = pow((N(3.0,0.0) - 3.0*N(1.0,2.0)), 2.0) + pow((3.0*N(2.0,1.0) - N(0.0,3.0)), 2.0);			
			double M4 = pow(N(3.0, 0.0) + N(1.0, 2.0), 2) + pow(N(2.0, 1.0) + N(0.0, 3.0), 2);
			double M7 = N(2.0,0.0)*N(0.0,2.0) - pow(N(1.0,1.0),2.0);			
			double M8 = N(3.0,0.0)*N(1.0,2.0) + N(2.0, 1.0)*N(0.0,3.0) - pow(N(1.0, 2.0), 2) - pow(N(2.0, 1.0), 2);

			ss.str(""); ss.clear();
			ss << std::fixed << std::setprecision(10) << std::showpoint 
				<< "M1 = " << M1
				<< "; M2 = " << M2
				<< "; M3 = " << M3
				<< "; M4 = " << M4 
				<< "; M7 = " << M7 
				<< "; M8 = " << M8
				<< ";\r\n";
			trace(ss.str().c_str());
			RecolorSegment(rows, width, height, segment_color, SEGMENTS[big++%6]);
		} else {
			RecolorSegment(rows, width, height, segment_color, BLACK);
		}
	}
}

void CDibDoc::ConvertToBlockedImage(CView * view)
{	
	BnWCalculate(view, 255);
}
