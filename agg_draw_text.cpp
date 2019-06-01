//#include <Windows.h>

#include "agg_draw_text.h"

//font_engine_type m_feng(GetDC(NULL));
font_engine_type m_feng;
font_manager_type m_fman(m_feng);

// Pipeline to process the vectors glyph paths (curves + contour)
agg::conv_curve<font_manager_type::path_adaptor_type> m_curves(m_fman.path_adaptor());
agg::conv_contour<agg::conv_curve<font_manager_type::path_adaptor_type> > m_contour(m_curves);