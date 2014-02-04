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
#include <map>
#include <vector>

#include "dibdoc.h"
#include "dibview.h"
#include "outDoc.h"
#include "afxext.h"

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

bool last_direction;

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
	return gray(px) > 205;
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
	RGBTRIPLE segment_color = { 127, 0, 0 };
	for (int x = 0, y = 0; y < height; ++y)
		rows[y][x] = BLACK;
	for (int x = 0, y = 0; x < width; ++x)
		rows[y][x] = BLACK;

	for (int x = 1; x < width; x += 4)
	for (int y = 1; y < height; y += 4) {
		const RGBTRIPLE this_color = rows[y][x];
		if (this_color != WHITE)
			continue;
		Recolor(rows, x, y, width, height, getNextSegmentColor(segment_color));
		segments++;
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

char letter(double M[11]) {
	if (M[5] > 0.000001)
		return 'T';
	if (M[9] < -0.00002)
		return 'A';
	if (M[2] > 0.025)
		return 'S';
	if ((M[9] < -0.000001 && M[3] > 0.0001) || M[8] < -0.00005)
		return 'U';
	if (M[6] > 0.0000002 || M[4] > 0.00001)
		return 'R';
	return 'N';
}

double L(RGBTRIPLE** rows, int x1, int y1, int x2, int y2, int width, int height, RGBTRIPLE color) {
  // OBWOD
  int l = 0;
  for (int x = x1; x <= x2; ++x)
  for (int y = y1; y <= y2; ++y)
    if (rows[y][x] == color &&
      (rows[max(0, y)][max(0, x - 1)] != color ||
      rows[min(height, y)][min(width, x + 1)] != color ||
      rows[max(0, y - 1)][max(0, x)] != color ||
      rows[min(height, y + 1)][min(width, x)] != color)) {
      ++l;
      //rows[y][x] = red;
    }
  return l;  
}

RGBTRIPLE lColor(char l) {
	switch(l) {
	case 'S': return SEGMENTS[0];
	case 'A': return SEGMENTS[1];
	case 'T': return SEGMENTS[2];
	case 'U': return SEGMENTS[3];
	case 'R': return SEGMENTS[4];
	case 'N': return SEGMENTS[5];
	default:
		return BLACK;
	}
}


struct SegmentData {
	RGBTRIPLE color;
	int x1,y1,x2,y2;
	double ax, ay;
	char letter;
};

struct SATURN {
	SegmentData S, A, T, U, R, N;
	void boundingRect(int& x1, int& y1, int& x2, int& y2) {
		x1 = S.x1; x2 = S.x2; y1 = S.y1; y2 = S.y2;
		x1 = min(x1, A.x1); x2 = max(x2, A.x2); y1 = min(y1, A.y1); y2 = max(y2, A.y2);
		x1 = min(x1, T.x1); x2 = max(x2, T.x2); y1 = min(y1, T.y1); y2 = max(y2, T.y2);
		x1 = min(x1, U.x1); x2 = max(x2, U.x2); y1 = min(y1, U.y1); y2 = max(y2, U.y2);
		x1 = min(x1, R.x1); x2 = max(x2, R.x2); y1 = min(y1, R.y1); y2 = max(y2, R.y2);
		x1 = min(x1, N.x1); x2 = max(x2, N.x2); y1 = min(y1, N.y1); y2 = max(y2, N.y2);
	}
};


#define m(p,q) mpq(rows,x1,y1,x2,y2,current_color,(p),(q))
#define M(p,q) Mpq(rows,x1,y1,x2,y2,ax,ay,current_color,(p),(q))
#define N(p,q) M(((double)p),((double)q))/pow(m(0,0), ((((double)p)+((double)q))/2) + 1)
// THIS DEFINES FOLOWING VARIABLES FOR GIVEN SEGMENT ACTIVATING USAGE OF m, M and N macros
// int x1,y1,x2,y2 - bounding rectangle of segment
// ax, ay - the center of the gravity of the segment
// sm - field of segment in pixels
// current_color - collor of current segment
#define PREPARE_MOMENTUM_MACROS(segment_color) \
	int x1, y1, x2, y2; \
	double ay, ax, sm; \
	RGBTRIPLE current_color = segment_color; \
	BoundingRect(rows, width, height, &x1, &y1, &x2, &y2, (current_color)); \
	sm = m(0,0); \
	ay = m(0,1)/sm; \
	ax = m(1,0)/sm; \


bool isBetween(const SegmentData& what, const SegmentData& first, const SegmentData& second) {
	double a, b, d;
	if (what.ax <= first.ax || what.ax >= second.ax)
		return false;
	a = (second.ay - first.ay)/(second.ax - first.ax);
	b = first.ay - first.ax*a;
	d = std::abs(-a*what.ax + what.ay - b)/sqrt(pow(a,2) + 1);	
	return d < 5.0;
}

void drawBR(RGBTRIPLE** rows, int x1, int y1, int x2, int y2) {
	for (int x = x1; x <= x2; ++x) {
		rows[y1][x] = SEGMENTS[0];
		rows[y1-1][x] = SEGMENTS[0];
		rows[y2][x] = SEGMENTS[0];
		rows[y2+1][x] = SEGMENTS[0];
	}
	for (int y = y1; y <= y2; ++y) {
		rows[y][x1] = SEGMENTS[0];
		rows[y][x1-1] = SEGMENTS[0];
		rows[y][x2] = SEGMENTS[0];
		rows[y][x2+1] = SEGMENTS[0];
	}
}

bool isNear(SegmentData first, SegmentData second) {
	if (!last_direction)
		std::swap(first, second);
	double dst_max = (0.5 * (((first.x2 - first.x1) + (second.x2 - second.x1))/2.0)); 
	return (second.x1 - first.x2) < dst_max;
}

void CDibDoc::BnWCalculate(CView * view, int green_value) {
	std::stringstream ss;
	std::map<char, std::vector<SegmentData>> letter_segments;
	std::vector<SATURN> saturns;
	RGBTRIPLE* rows_origin[1000];
	RGBTRIPLE* rows[1000];
	int width, height;	
#ifdef BACKGR
	if (!GetDIBRowsRGB(m_hDIB,rows_origin,&width,&height))
		return;
	for (int i = 0; i < 1000; ++i)
		rows[i] = new RGBTRIPLE[1000];
	for (int x = 0; x < width; ++x)
	for (int y = 0; y < height; ++y)
		rows[y][x] = rows_origin[y][x];
#else
	if (!GetDIBRowsRGB(m_hDIB,rows,&width,&height))
		return;
#endif
	ToBnW(rows, width, height);
	int segments_found = SegmentsMarker(rows, width, height);
	RGBTRIPLE segment_color = { 127, 0, 0 };
	int big = 0;
	for (int i = 0; i < segments_found; ++i) {
		PREPARE_MOMENTUM_MACROS(getNextSegmentColor(segment_color));
		if (sm > 15) { // SEGMENTS BELOW 16px ARE TREATED AS NOISE
			double NM[11]; //ZERO WILL NOT BE USED
			NM[1] = N(2, 0) + N(0, 2);
			NM[2] = pow(N(2,0) - N(0,2), 2) + 4 * pow(N(1,1), 2);			
			NM[3] = pow(N(3,0) - 3*N(1,2), 2) + pow(3*N(2,1) - N(0,3), 2);			
			NM[4] = pow(N(3,0) + N(1,2), 2) + pow(N(2,1) + N(0,3), 2);
			NM[5] = (N(3,0) - 3*N(1,2))*(N(3,0) + N(1,2))*(pow(N(3,0) + N(1,2), 2)- 3*pow(N(2,1) + N(0,3), 2)) + (3*N(2,1) - N(0,3))*(N(2,1)+N(0,3))*(3*pow(N(3,0) + N(1,2), 2) - pow(N(2,1) + N(0,3), 2));
			NM[6] = (N(2,0) - N(0,2))*(pow(N(3,0) + N(1,2), 2) -pow(N(2,1) + N(0,3),2)) + 4*N(1,1)*(N(3,0) + N(1,2))*(N(2,1) + N(0,3));
			NM[7] = N(2,0)*N(0,2) - pow(N(1,1),2);			
			NM[8] = N(3,0)*N(1,2) + N(2,1)*N(0,3) - pow(N(1,2), 2) - pow(N(2,1), 2);
			NM[9] = N(2,0)*(N(2,1)*N(0,3) - pow(N(1,2), 2)) + N(0,2)*(N(3,0)*N(1,2) - pow(N(2,1), 2)) - N(1,1)*(N(3,0)*N(0,3) - N(2,1)*N(1,2));
			NM[10] = pow(N(3,0)*N(0,3) - N(1,2)*N(2,1), 2) - 4*(N(3,0)*N(1,2) - pow(N(2,1), 2))*(N(0,3)*N(2,1) - N(1,2));
			char leter = letter(NM);
			SegmentData data = { segment_color, x1, y1, x2, y2, ax, ay, leter };
			letter_segments[leter].push_back(data);
			RecolorSegment(rows, width, height, segment_color, lColor(leter));

			ss.str(""); ss.clear();
			ss << leter << "\r\n";
			trace(ss.str().c_str());
		} else {
			RecolorSegment(rows, width, height, segment_color, BLACK);
		}
	}
	
	// FIND WHOLE SATURN's
	for (const SegmentData& S : letter_segments['S']) {
	for (const SegmentData& N : letter_segments['N']) {
		bool direction = N.ax > S.ax;
		last_direction = direction;
	for (const SegmentData& A : letter_segments['A']) {
		if (direction ? !isBetween(A, S, N) : !isBetween(A, N, S))
			continue;
		if (!isNear(S, A))
			continue;
	for (const SegmentData& T : letter_segments['T']) {
		if (direction ? !isBetween(T, A, N) : !isBetween(T, N, A))
			continue;
		if (!isNear(A, T))
			continue;
	for (const SegmentData& U : letter_segments['U']) {
		if (direction ? !isBetween(U, T, N) : !isBetween(U, N, T))
			continue;
		if (!isNear(T, U))
			continue;
	for (const SegmentData& R : letter_segments['R']) {
		if (direction ? !isBetween(R, U, N) : !isBetween(R, N, U))
			continue;
		if (!isNear(U, R))
			continue;
		if (!isNear(R, N))
			continue;
		// YEY, WHOLE SATURN FOUND
		SATURN saturn = {S,A,T,U,R,N};
		saturns.push_back(saturn);
	}
	}
	}
	}
	}
	}
	rows[0][0] = WHITE;
	for (SATURN& s : saturns) {
		RecolorSegment(rows, width, height, s.S.color, SEGMENTS[0]);
		RecolorSegment(rows, width, height, s.A.color, SEGMENTS[1]);
		RecolorSegment(rows, width, height, s.T.color, SEGMENTS[2]);
		RecolorSegment(rows, width, height, s.U.color, SEGMENTS[3]);
		RecolorSegment(rows, width, height, s.R.color, SEGMENTS[4]);
		RecolorSegment(rows, width, height, s.N.color, SEGMENTS[5]);
		int x1,y1,x2,y2;
		s.boundingRect(x1,y1,x2,y2);
#ifdef BACKGR
		drawBR(rows_origin, x1, y1, x2, y2);
#else
		drawBR(rows, x1, y1, x2, y2);
#endif
	}
	ss.str(""); ss.clear();
	ss << saturns.size() << "\r\n";
	trace(ss.str().c_str());
}

void CDibDoc::ConvertToBlockedImage(CView * view)
{	
	BnWCalculate(view, 255);
}
