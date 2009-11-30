#include "../include/rasterizer.h"

#include "../include/shaderregs_op.h"
#include "../include/clipper.h"
#include "../include/framebuffer.h"
#include "../include/renderer_impl.h"

#include "eflib/include/slog.h"

#include <algorithm>
#include <boost/format.hpp>

using namespace std;
using namespace efl;
using namespace boost;

struct scanline_info
{
	size_t scanline_width;

	vs_output_impl ddx;

	vs_output_impl base_vert;
	size_t base_x;
	size_t base_y;

	scanline_info()
	{}

	scanline_info(const scanline_info& rhs)
		:ddx(rhs.ddx), base_vert(rhs.base_vert), scanline_width(scanline_width),
		base_x(rhs.base_x), base_y(rhs.base_y)
	{}

	scanline_info& operator = (const scanline_info& rhs){
		ddx = rhs.ddx;
		base_vert = rhs.base_vert;
		base_x = rhs.base_x;
		base_y = rhs.base_y;
	}
};

//inherited
void rasterizer::initialize(renderer_impl* pparent)
{
	pparent_ = pparent;
	hfb_ = pparent->get_framebuffer();
	hps_ = pparent->get_pixel_shader();
}

IMPL_RS_UPDATED(rasterizer, pixel_shader)
{
	hps_ = pparent_->get_pixel_shader();
	return result::ok;
}

/*************************************************
 *   �߶εĹ�դ�����裺
 *			1 Ѱ�������򣬻��������������������������ϵĲ��
 *			2 ����ddx��ddy������mip��ѡ��
 *			3 �����������������ּ�������λ�ü�vs_output_impl
 *			4 ִ��pixel shader
 *			5 ��������Ⱦ��framebuffer��
 *
 *   Note: 
 *			1 ������postion��λ�ڴ�������ϵ��
 *			2 wpos��x y z�����Ѿ�������clip w
 *			3 positon.wΪ1.0f / clip w
 **************************************************/
void rasterizer::rasterize_line_impl(const vs_output_impl& v0, const vs_output_impl& v1)
{
	vs_output_impl diff = project(v1) - project(v0);
	const efl::vec4& dir = diff.wpos;
	float diff_dir = abs(dir.x) > abs(dir.y) ? dir.x : dir.y;

	//������
	vs_output_impl derivation = diff;

	vs_output_impl ddx = diff * (diff.wpos.x / (diff.wpos.xy().length_sqr()));
	vs_output_impl ddy = diff * (diff.wpos.y / (diff.wpos.xy().length_sqr()));

	ps_output px_out;

	//��Ϊx major��y majorʹ��DDA������
	if( abs(dir.x) > abs(dir.y))
	{

		//�������յ㣬ʹ�������
		const vs_output_impl *start, *end;
		if(dir.x < 0){
			start = &v1;
			end = &v0;
		} else {
			start = &v0;
			end = &v1;
		}

		triangle_info info;
		info.set(start->wpos, ddx, ddy);
		hps_->ptriangleinfo_ = &info;

		float fsx = floor(start->wpos.x + 0.5f);

		int sx = int(fsx);
		int ex = int(floor(end->wpos.x - 0.5f));

		//��ȡ����Ļ��
		sx = efl::clamp<int>(sx, 0, int(hfb_->get_width() - 1));
		ex = efl::clamp<int>(ex, 0, int(hfb_->get_width()));

		//��������vs_output_impl
		vs_output_impl px_start(project(*start));
		vs_output_impl px_end(project(*end));
		float step = fsx + 0.5f - start->wpos.x;
		vs_output_impl px_in = lerp(px_start, px_end, step / diff_dir);

		//x-major ���߻���
		for(int iPixel = sx; iPixel < ex; ++iPixel)
		{
			//���Բ���vp��Χ�ڵ�����
			if(px_in.wpos.y >= hfb_->get_height()){
				if(dir.y > 0) break;
				continue;
			}
			if(px_in.wpos.y < 0){
				if(dir.y < 0) break;
				continue;
			}

			//����������Ⱦ
			if(hps_->execute(unproject(px_in), px_out)){
				hfb_->render_pixel(iPixel, int(px_in.wpos.y), px_out);
			}

			//��ֵ���
			++ step;
			px_in = lerp(px_start, px_end, step / diff_dir);
		}
	}
	else //y major
	{
		//�����������ݷ���
		const vs_output_impl *start, *end;
		if(dir.y < 0){
			start = &v1;
			end = &v0;
		} else {
			start = &v0;
			end = &v1;
		}

		triangle_info info;
		info.set(start->wpos, ddx, ddy);
		hps_->ptriangleinfo_ = &info;

		float fsy = floor(start->wpos.y + 0.5f);

		int sy = int(fsy);
		int ey = int(floor(end->wpos.y - 0.5f));

		//��ȡ����Ļ��
		sy = efl::clamp<int>(sy, 0, int(hfb_->get_height() - 1));
		ey = efl::clamp<int>(ey, 0, int(hfb_->get_height()));

		//��������vs_output_impl
		vs_output_impl px_start(project(*start));
		vs_output_impl px_end(project(*end));
		float step = fsy + 0.5f - start->wpos.y;
		vs_output_impl px_in = lerp(px_start, px_end, (fsy + 0.5f - start->wpos.y) / diff_dir);

		//x-major ���߻���
		for(int iPixel = sy; iPixel < ey; ++iPixel)
		{
			//���Բ���vp��Χ�ڵ�����
			if(px_in.wpos.x >= hfb_->get_width()){
				if(dir.x > 0) break;
				continue;
			}
			if(px_in.wpos.x < 0){
				if(dir.x < 0) break;
				continue;
			}

			//����������Ⱦ
			if(hps_->execute(unproject(px_in), px_out)){
				hfb_->render_pixel(int(px_in.wpos.x), iPixel, px_out);
			}

			//��ֵ���
			++ step;
			px_in = lerp(px_start, px_end, step / diff_dir);
		}
	}
}

/*************************************************
*   �����εĹ�դ�����裺
*			1 ��դ������ɨ���߼�ɨ���߲����Ϣ
*			2 rasterizer_scanline_impl����ɨ����
*			3 ����������ص�vs_output_impl
*			4 ִ��pixel shader
*			5 ��������Ⱦ��framebuffer��
*
*   Note: 
*			1 ������postion��λ�ڴ�������ϵ��
*			2 wpos��x y z�����Ѿ�������clip w
*			3 positon.wΪ1.0f / clip w
**************************************************/
void rasterizer::rasterize_triangle_impl(const vs_output_impl& v0, const vs_output_impl& v1, const vs_output_impl& v2)
{
	typedef slog<text_log_serializer> slog_type;
	log_serializer_indent_scope<log_system<slog_type>::slog_type> scope(&log_system<slog_type>::instance());

	//��¼�����ε���Ļ����ϵ���㡣
	log_system<slog_type>::instance().write(_EFLIB_T("wv0"),
		to_tstring(str(format("( %1%, %2%, %3%)") % v0.wpos.x % v0.wpos.y % v0.wpos.z)), LOGLEVEL_MESSAGE
		);
	log_system<slog_type>::instance().write(_EFLIB_T("wv1"), 
		to_tstring(str(format("( %1%, %2%, %3%)") % v1.wpos.x % v1.wpos.y % v1.wpos.z)), LOGLEVEL_MESSAGE
		);
	log_system<slog_type>::instance().write(_EFLIB_T("wv2"), 
		to_tstring(str(format("( %1%, %2%, %3%)") % v2.wpos.x % v2.wpos.y % v2.wpos.z)), LOGLEVEL_MESSAGE
		);

	/**********************************************************
	*        �����㰴��y��С�������������������
	**********************************************************/
	const vs_output_impl* pvert[3] = {&v0, &v1, &v2};

	//��������
	if(pvert[0]->wpos.y > pvert[1]->wpos.y){
		swap(pvert[1], pvert[0]);
	}
	if(pvert[1]->wpos.y > pvert[2]->wpos.y){
		swap(pvert[2], pvert[1]);
		if(pvert[0]->wpos.y > pvert[1]->wpos.y) 
			swap(pvert[1], pvert[0]);
	}

	//��ʼ���߼��������ԵĲ�
	vs_output_impl e01 = project(*(pvert[1])) - project(*(pvert[0]));
	//float watch_x = e01.attributes[2].x;
	
	vs_output_impl e02 = project(*(pvert[2])) - project(*(pvert[0]));
	vs_output_impl e12;



	//��ʼ�����ϵĸ���������ֵ����ֻҪ�������߾Ϳ����ˡ���
	e12.wpos = pvert[2]->wpos - pvert[1]->wpos;

	//��ʼ��dxdy
	float dxdy_01 = efl::equal<float>(e01.wpos.y, 0.0f) ? 0.0f: e01.wpos.x / e01.wpos.y;
	float dxdy_02 = efl::equal<float>(e02.wpos.y, 0.0f) ? 0.0f: e02.wpos.x / e02.wpos.y;
	float dxdy_12 = efl::equal<float>(e12.wpos.y, 0.0f) ? 0.0f: e12.wpos.x / e12.wpos.y;

	//�������
	float area = cross_prod2(e02.wpos.xy(), e01.wpos.xy());
	float inv_area = 1.0f / area;
	if(equal<float>(area, 0.0f)) return;

	/**********************************************************
	*  ���������ԵĲ��ʽ
	*********************************************************/
	vs_output_impl ddx((e02 * e01.wpos.y - e02.wpos.y * e01)*inv_area);
	vs_output_impl ddy((e01 * e02.wpos.x - e01.wpos.x * e02)*inv_area);

	triangle_info info;
	info.set(pvert[0]->wpos, ddx, ddy);
	hps_->ptriangleinfo_ = &info;

	/*************************************
	*   ���û�����scanline���ԡ�
	*   ��Щ���Խ��ڶ��ɨ�����б�����ͬ
	************************************/
	scanline_info base_scanline;
	base_scanline.ddx = ddx;

	/*************************************************
	*   ��ʼ���ƶ���Ρ��������-�·ָ�����㷨
	*   ��ɨ���߹�դ����֤���������ҵġ�
	*   ���Բ���Ҫ����major edge����������ұߡ�
	*************************************************/

	const int bot_part = 0;
	//const int top_part = 1;

	for(int iPart = 0; iPart < 2; ++iPart){

		//�����ߵ�dxdy
		float dxdy0 = 0.0f;
		float dxdy1 = 0.0f;

		//��ʼ/��ֹ��x��y����; 
		//�������λ�׼��λ��,���ڼ��㶥������
		float
			fsx0(0.0f), fsx1(0.0f), // ��׼�����ֹx����
			fsy(0.0f), fey(0.0f),	// Part����ֹɨ����y����
			fcx0(0.0f), fcx1(0.0f), // ����ɨ���ߵ���ֹ������
			offsety(0.0f);

		int isy(0), iey(0);	//Part����ֹɨ���ߺ�

		//ɨ���ߵ���ֹ����
		const vs_output_impl* s_vert = NULL;
		const vs_output_impl* e_vert = NULL;

		//����Ƭ��������ʼ����
		if(iPart == bot_part){
			s_vert = pvert[0];
			e_vert = pvert[1];

			dxdy0 = dxdy_01;
			dxdy1 = dxdy_02;
		} else {
			s_vert = pvert[1];
			e_vert = pvert[2];

			dxdy0 = dxdy_12;
			dxdy1 = dxdy_02;
		}

		if(equal<float>(s_vert->wpos.y, e_vert->wpos.y)){
			continue; // next part
		}

		fsy = ceil(s_vert->wpos.y + 0.5f) - 1;
		fey = ceil(e_vert->wpos.y - 0.5f) - 1;

		isy = int(fsy);
		iey = int(fey);

		offsety = fsy + 0.5f - pvert[0]->wpos.y;

		//����x�������������εĲ�ͬ��������ͬ
		if(iPart == bot_part){
			fsx0 = pvert[0]->wpos.x + dxdy_01*(fsy + 0.5f - pvert[0]->wpos.y);
			fsx1 = pvert[0]->wpos.x + dxdy_02*(fsy + 0.5f - pvert[0]->wpos.y);
		} else {
			fsx0 = pvert[1]->wpos.x + dxdy_12*(fsy +0.5f - pvert[1]->wpos.y);
			fsx1 = pvert[0]->wpos.x + dxdy_02*(fsy +0.5f - pvert[0]->wpos.y);
		}

		//���û�׼ɨ���ߵ�����
		project(base_scanline.base_vert, *(pvert[0]));
		integral(base_scanline.base_vert, offsety, ddy);

		//��ǰ�Ļ�׼ɨ���ߣ������(base_vert.x, scanline.y)����
		//�ڴ��ݵ�rasterize_scanline֮ǰ��Ҫ�������������ɨ���ߵ�����ˡ�
		scanline_info current_base_scanline(base_scanline);

		const viewport& vp = pparent_->get_viewport();

		int vpleft = int(max(0, vp.x));
		int vpbottom = int(max(0, vp.y));
		int vpright = int(min(vp.x+vp.w, hfb_->get_width()));
		int vptop = int(min(vp.y+vp.h, hfb_->get_height()));

		for(int iy = isy; iy <= iey; ++iy)
		{	
			//���ɨ������view port��������������	
			if( iy >= vptop ){
				break;
			}

			if( iy >= vpbottom ){
				//ɨ�������ӿ��ڵľ���ɨ����
				int icx_s = 0;
				int icx_e = 0;

				fcx0 = dxdy0 * (iy - isy) + fsx0;
				fcx1 = dxdy1 * (iy - isy) + fsx1;

				//LOG: ��¼ɨ���ߵ���ֹ�㡣�汾
				//if (fcx0 > 256.0 && iy == 222)
				//{
				//	log_serializer_indent_scope<log_system<slog_type>::slog_type> scope(&log_system<slog_type>::instance());
				//	log_system<slog_type>::instance().write(
				//		to_tstring(str(format("%1%") % iy)), 
				//		to_tstring(str(format("%1$8.5f, %2$8.5f") % fcx0 % fcx1)), LOGLEVEL_MESSAGE
				//		);
				//}

				if(fcx0 < fcx1){
					icx_s = (int)ceil(fcx0 + 0.5f) - 1;
					icx_e = (int)ceil(fcx1 - 0.5f) - 1;
				} else {
					icx_s = (int)ceil(fcx1 + 0.5f) - 1;
					icx_e = (int)ceil(fcx0 - 0.5f) - 1;
				}

				icx_s = efl::clamp<int>(icx_s, vpleft, vpright - 1);
				icx_e = efl::clamp<int>(icx_e, vpleft, vpright - 1);

				//����������յ�˵��scanline�в������κ��������ģ�ֱ��������
				if(icx_s <= icx_e) {
					float offsetx = float(icx_s) + 0.5f - pvert[0]->wpos.x;

					//����ɨ������Ϣ
					scanline_info scanline(current_base_scanline);
					integral(scanline.base_vert, offsetx, ddx);

					scanline.base_x = icx_s;
					scanline.base_y = iy;
					scanline.scanline_width = icx_e - icx_s + 1;

					//��դ��
					rasterize_scanline_impl(scanline);
				}
			}

			//��ֵ���
			integral(current_base_scanline.base_vert, 1.0f, ddy);
		}
	}
}

//ɨ���߹�դ�����򣬽���ɨ�������ݲ����Ϣ���й�դ��������դ����Ƭ�δ��ݵ�������ɫ����.
//Note:��������ؽ�w�˻ص�attribute��.
void rasterizer::rasterize_scanline_impl(const scanline_info& sl)
{
	vs_output_impl px_in(sl.base_vert);
	ps_output px_out;

	for(size_t i_pixel = 0; i_pixel < sl.scanline_width; ++i_pixel)
	{
		//if(px_in.wpos.z <= 0.0f)
			//continue;

		//ִ��shader program
		if(hps_->execute(unproject(px_in), px_out) ){
			hfb_->render_pixel(sl.base_x + i_pixel, sl.base_y, px_out);
		}

		integral(px_in, 1.0f, sl.ddx);
	}
}

rasterizer::rasterizer()
{
	cm_ = cull_back;
	fm_ = fill_solid;
}

void rasterizer::rasterize_line(const vs_output_impl& v0, const vs_output_impl& v1)
{
	//�����ȫ�����߽磬���޳�
	const viewport& vp = pparent_->get_viewport();

	if(v0.wpos.x < 0 && v1.wpos.x < 0) return;
	if(v0.wpos.y < 0 && v1.wpos.y < 0) return;
	if(v0.wpos.z < vp.minz && v1.wpos.z < vp.minz) return;

	if(v0.wpos.x >= vp.w && v1.wpos.x >= vp.w) return;
	if(v0.wpos.y >= vp.w && v1.wpos.y >= vp.w) return;
	if(v0.wpos.z >= vp.maxz && v1.wpos.z >= vp.maxz) return;

	//render
	rasterize_line_impl(v0, v1);
}

void rasterizer::rasterize_triangle(const vs_output_impl& v0, const vs_output_impl& v1, const vs_output_impl& v2)
{
	//�߽��޳�
	const viewport& vp = pparent_->get_viewport();
	
	//�����޳�
	if(cm_ != cull_none){
		float area = compute_area(v0, v1, v2);
		if( (cm_ == cull_front) && (area > 0) ){
			return;
		}
		if( (cm_ == cull_back) && (area < 0) ){
			return;
		}
	}

	//��Ⱦ
	if(fm_ == fill_wireframe){
		rasterize_line(v0, v1);
		rasterize_line(v1, v2);
		rasterize_line(v0, v2);
	} else {
		h_clipper clipper = pparent_->get_clipper();
		clipper->set_viewport(vp);
		const vector<const vs_output_impl*>& clipped_verts = clipper->clip(v0, v1, v2);

		for(size_t i_tri = 1; i_tri < clipped_verts.size() - 1; ++i_tri){
			rasterize_triangle_impl(*clipped_verts[0], *clipped_verts[i_tri], *clipped_verts[i_tri+1]);
		}
		//rasterize_triangle_impl(v0, v1, v2);
	}
}

void rasterizer::set_cull_mode(cull_mode cm)
{
	cm_ = cm;
}

void rasterizer::set_fill_mode(fill_mode fm)
{
	fm_ = fm;
}