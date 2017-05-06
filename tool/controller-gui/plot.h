#pragma once

#include<Windows.h>
#include<afxwin.h>

#include<vector>
#include<functional>
#include<type_traits>
#include<memory>
#include<string>
#include<sstream>
#include<cassert>

namespace plot
{




	// Utility




	template <typename T> using Invoke =
		typename T::type;
	template <typename Condition> using EnableIf =
        Invoke < std::enable_if<Condition::value> > ;

    template<typename T> inline void empty_deleter(T *) {}





	// point





	template<
		typename N,
		typename = EnableIf<std::is_arithmetic<N>>
	>
	struct point
	{
		using value_type = N;

		N x, y;

		point(N x, N y)
			: x(x), y(y)
		{
		}

		point()
			: point(0, 0)
		{
		}

		operator POINT () const
		{
			return{ x, y };
		}
	};








	
	
	// rect







	template<
		typename N,
		typename = EnableIf<std::is_arithmetic<N>>
	>
	struct rect
	{
		using value_type = N;

		N xmin, xmax, ymin, ymax;

		rect(N xmin, N xmax, N ymin, N ymax)
			: xmin(xmin), xmax(xmax), ymin(ymin), ymax(ymax)
		{
		}

		rect()
			: rect(0, 0, 0, 0)
		{
		}

		N width() const
		{
			return xmax - xmin;
		}

		N height() const
		{
			return ymax - ymin;
		}

		operator RECT () const
		{
			return {
				xmin, ymin, xmax, ymax
			};
		}

		rect<N> & operator=(const rect<N> &other)
		{
			this->xmin = other.xmin;
			this->xmax = other.xmax;
			this->ymin = other.ymin;
			this->ymax = other.ymax;
			return *this;
		}
	};

	using world_t =  rect<double>;
	using screen_t = rect<int>;








	// translator






	template<
		typename NS,
		typename NT,
		typename PS = rect < NS >,
		typename PT = rect < NT >
	>
	class translator
	{

		const PS &source;
		const PT &target;

	public:

		translator(const PS &source, const PT &target) : source(source), target(target)
		{
		}

	private:

		inline double scale_x() const
		{
			return
				double(target.width()) /
				double(source.width());
		}

		inline double translation_x() const
		{
			return
				double(target.xmin) - scale_x() * double(source.xmin);
		}

		inline double scale_y() const
		{
			return -
				double(target.height()) /
				double(source.height());
		}

		inline double translation_y() const
		{
			return
				double(target.ymax) - scale_y() * double(source.ymin);
		}

	public:

		inline NT x(NS x) const
		{
			return NT(scale_x() * x + translation_x());
		}

		inline NT y(NS y) const
		{
			return NT(scale_y() * y + translation_y());
		}

		inline point<NT> xy(const point<NS> &xy) const
		{
			return{
				x(xy.x),
				y(xy.y)
			};
		}
	};





	
	
	
	// viewport







	class viewport
	{

		using s_to_w_t = translator < screen_t::value_type, world_t::value_type > ;
		using w_to_s_t = translator < world_t::value_type, screen_t::value_type > ;

	public:

		const screen_t screen;
		const world_t  world;

	private:

		const s_to_w_t s_to_w;
		const w_to_s_t w_to_s;

	public:

		viewport(const screen_t &screen, const world_t &world)
			: screen(screen), world(world),
			s_to_w(this->screen, this->world),
			w_to_s(this->world, this->screen)
		{
		}

		viewport(const viewport &other)
			: viewport(other.screen, other.world)
		{
		}

		const s_to_w_t & screen_to_world() const
		{
			return s_to_w;
		}

		const w_to_s_t & world_to_screen() const
		{
			return w_to_s;
		}
	};







	// palette







	namespace palette
	{

		using pen_ptr   = std::shared_ptr<CPen>;
		using brush_ptr = std::shared_ptr<CBrush>;

		inline pen_ptr pen(COLORREF color = 0x000000, int width = 1, int style = 0)
		{
			return pen_ptr(new CPen(style, width, color));
		}

		inline brush_ptr brush(COLORREF color = 0x000000)
		{
			return brush_ptr(new CBrush(color));
		}
	};







	
	// drawable








	class drawable
	{

	public:

		virtual void draw(CDC &dc, const viewport &bounds) = 0;

		virtual ~drawable()
		{
		}
	};








	// custom_drawable







	class custom_drawable : public drawable
	{

	public:

		using painter_t = std::function < void(CDC &dc, const viewport &bounds) > ;

	private:

		painter_t painter;

	public:

		custom_drawable(painter_t painter)
			: painter(std::move(painter))
		{
		}

	public:

		virtual void draw(CDC &dc, const viewport &bounds) override
		{
			painter(dc, bounds);
		}
	};







	
	// layer_drawable








	class layer_drawable : public drawable
	{

	public:

		using layer_ptr = std::unique_ptr < drawable > ;

	private:

		using this_t = layer_drawable &;

		std::vector<layer_ptr> layers;

	public:

		this_t with(layer_ptr &&layer)
		{
			layers.push_back(std::move(layer));
			return *this;
		}

		this_t with(drawable *layer)
		{
			layers.emplace_back(layer);
			return *this;
		}

		virtual void draw(CDC &dc, const viewport &bounds) override
		{
			for each (auto &layer in layers)
			{
				int _old = dc.SaveDC();

				RECT clip = bounds.screen;
				dc.IntersectClipRect(&clip);

				layer->draw(dc, bounds);

				dc.RestoreDC(_old);
			}
		}
	};









	// function_drawable








	class function_drawable : public drawable
	{

	public:

		using func_t  = std::function < double(double) > ;
		using pen_ptr = palette::pen_ptr;

	private:

		func_t  f;
		pen_ptr pen;

	public:

		function_drawable(func_t f, pen_ptr &&pen = {})
			: f(std::move(f))
			, pen(std::move(pen))
		{
		}

		virtual void draw(CDC &dc, const viewport &bounds) override
		{
			if (pen)
			{
				dc.SelectObject(pen.get());
			}
			for (int i = bounds.screen.xmin, j = INT_MIN, j0; i < bounds.screen.xmax; i++)
			{
				j0 = j;
				j = bounds.world_to_screen().y(
					f(bounds.screen_to_world().x(i))
				);
				if (j0 != INT_MIN)
				{
					dc.LineTo(i, j);
				}
				else
				{
					dc.MoveTo(i, j);
				}
			}
		}
	};









	// list_drawable









	class list_drawable : public drawable
	{

	public:

		using point_t        = point < double > ;
        using data_t         = std::vector < point_t > ;
        using data_factory_t = std::function < std::shared_ptr<data_t> (const viewport &bounds) > ;
		using painter_t      = std::function < void(CDC &dc, const viewport &bounds, const point<int> &at) > ;

        static data_factory_t const_data_factory(std::shared_ptr<data_t> data)
        {
            return [data](const viewport &bounds) { return std::move(data); };
        }

        static data_factory_t function_data_factory(function_drawable::func_t f, double origin, double interval)
        {
            return [f, origin, interval] (const viewport &bounds) {
                
                double size = bounds.world.width(), min = bounds.world.xmin, max = bounds.world.xmax;

                int n = int(size / interval);

                auto data = std::make_shared<data_t>();
                data->reserve(n + 2);

                int d = (int) std::ceil((origin - min) / interval);

                double start = origin - d * interval;

                int i = 0;
                for (double tick = start; tick < max; tick += interval, ++i)
                {
                    if (tick <= min) continue;
                    data->emplace_back(tick, f(tick));
                }

                return std::move(data);
            };
        }

	public:

		static list_drawable::painter_t circle_painter(
			int radius = 5,
			palette::brush_ptr &&brush = palette::brush())
		{
			return [brush, radius](CDC &dc, const viewport &bounds, const point<int> &at)
			{
				CBrush *_old = dc.SelectObject(brush.get());
                CPen p(PS_NULL, 1, RGB(0, 0, 0));
                CPen *_old_pen = dc.SelectObject(&p);
				RECT rct;
				rct.left = at.x - radius;
				rct.top = at.y - radius;
				rct.right = at.x + radius;
				rct.bottom = at.y + radius;
                dc.Ellipse(&rct);
                dc.SelectObject(_old_pen);
				dc.SelectObject(_old);
			};
		}

	private:

		data_factory_t   data_factory;
        painter_t        painter;

        palette::pen_ptr line_pen;

	public:

        list_drawable(data_factory_t data_factory, painter_t painter = circle_painter(), palette::pen_ptr line_pen = {})
			: data_factory(std::move(data_factory))
			, painter(std::move(painter))
            , line_pen(line_pen)
		{
		}

		virtual void draw(CDC &dc, const viewport &bounds) override
		{
            auto data = data_factory(bounds);
            if (data->empty()) return;
            dc.MoveTo(bounds.world_to_screen().xy((*data)[0]));
			for each (auto &p in *data)
			{
                if (line_pen)
                {
                    auto _old = dc.SelectObject(*line_pen);
                    dc.LineTo(bounds.world_to_screen().xy(p));
                    dc.SelectObject(_old);
                }
				painter(dc, bounds, bounds.world_to_screen().xy(p));
			}
		}
	};








	// re_viewporter










	class re_viewporter : public drawable
	{

		using layer_ptr		  = layer_drawable::layer_ptr;

    public:

		using viewport_mapper = std::function < std::shared_ptr<viewport> (const viewport&) > ;

    private:

		layer_ptr	    layer;
		viewport_mapper mapper;

	public:

		re_viewporter(layer_ptr &&layer, viewport_mapper mapper)
			: layer(std::move(layer))
			, mapper(std::move(mapper))
		{
		}

		virtual void draw(CDC &dc, const viewport &bounds) override
		{
			auto v = mapper(bounds);
			layer->draw(dc, *v);
		}
	};

	inline re_viewporter::viewport_mapper re_world(
		const world_t &world)
	{
		return
            [world](const viewport &bounds)
		    {
			    return std::make_shared<viewport>(bounds.screen, world);
		    };
	}

	inline re_viewporter::viewport_mapper re_screen(
		const screen_t &screen)
	{
		return
            [screen](const viewport &bounds)
		    {
			    return std::make_shared<viewport>(screen, bounds.world);
		    };
	}

	inline re_viewporter::viewport_mapper re_viewport(
		const viewport &vp)
	{
        auto shared = std::make_shared<viewport>(vp);
		return
            [shared](const viewport &bounds)
		    {
			    return std::move(shared);
		    };
	}








	// tick_factory







	class tick_factory
	{

	public:

		using label_t	  = std::string;
		using tick_t	  = std::pair<double, label_t>;
		using ticks_t	  = std::vector<tick_t>;
		using formatter_t = std::function<label_t(double)>;

	protected:

		formatter_t fmt;

	public:

		tick_factory(formatter_t fmt)
			: fmt(std::move(fmt))
		{
		}

		virtual ticks_t produce(const world_t &world) = 0;

		virtual ~tick_factory()
		{
		}

	};







	// const_interval_tick_factory







	enum class axe
	{
		x, y
	};

	template<axe x_or_y>
	class const_interval_tick_factory : public tick_factory
	{
		
		double origin;
		double interval;

	public:

		const_interval_tick_factory(formatter_t fmt,
									double origin,
									double interval)
			: tick_factory(std::move(fmt))
			, origin(origin)
			, interval(interval)
		{
		}
		
		virtual ticks_t produce(const world_t &world) override
		{
			double size, min, max;

			if (axe::x == x_or_y)
			{
				size = world.width();
				min = world.xmin;
				max = world.xmax;
			}
			else
			{
				size = world.height();
				min = world.ymin;
				max = world.ymax;
			}
			
			int n = int(size / interval);

			ticks_t ticks;
			ticks.reserve(n + 2);
			
			int d = (int) std::ceil((origin - min) / interval);
			
			double start = origin - d * interval;

			int i = 0;
			for (double tick = start; tick < max; tick += interval, ++i)
			{
                if (tick <= min) continue;
				ticks.emplace_back(tick, fmt(tick));
			}
			
			return ticks;
		}
	};

    template<axe x_or_y>
    class const_n_tick_factory : public tick_factory
    {

        double origin;
        int n_intervals;

    public:

        const_n_tick_factory(formatter_t fmt,
                             double origin,
                             int n_intervals)
                             : tick_factory(std::move(fmt))
                             , origin(origin)
                             , n_intervals(n_intervals)
        {
        }

        virtual ticks_t produce(const world_t &world) override
        {
            double size, min, max;

            if (axe::x == x_or_y)
            {
                size = world.width();
                min = world.xmin;
                max = world.xmax;
            }
            else
            {
                size = world.height();
                min = world.ymin;
                max = world.ymax;
            }

            double interval = size / n_intervals;

            ticks_t ticks;
            ticks.reserve(n_intervals + 2);

            int d = (int) std::ceil((origin - min) / interval);

            double start = origin - d * interval;

            int i = 0;
            for (double tick = start; tick < max; tick += interval, ++i)
            {
                if (tick <= min) continue;
                ticks.emplace_back(tick, fmt(tick));
            }

            return ticks;
        }
    };

	inline tick_factory::formatter_t make_formatter(int precision)
	{
		return [precision](double tick) {
			std::ostringstream oss;
			oss.precision(precision);
			oss << std::fixed << tick;
			return std::move(oss.str());
		};
	}
	








	// tick_drawable










	class tick_drawable : public drawable
	{

		using layer_ptr = layer_drawable::layer_ptr;
		using tf_ptr    = std::shared_ptr < tick_factory > ;
		using ticks_t   = tick_factory::ticks_t;
		using pen_ptr   = palette::pen_ptr;

		tf_ptr xtf, ytf;

		layer_ptr insides;
		pen_ptr   pen;

	public:

		tick_drawable(layer_ptr &&insides, tf_ptr xtf, tf_ptr ytf, pen_ptr &&pen = {})
			: insides(std::move(insides))
			, xtf(std::move(xtf))
			, ytf(std::move(ytf))
			, pen(std::move(pen))
		{
		}

		virtual void draw(CDC &dc, const viewport &bounds) override
		{
			if (pen)
			{
				dc.SelectObject(pen.get());
			}

			ticks_t x_ticks = xtf->produce(bounds.world);
			ticks_t y_ticks = ytf->produce(bounds.world);

			CString str;
			CSize tbounds;
			int x_offset = -1;
			int y_offset = -1;

			screen_t new_screen = bounds.screen;

			for each (auto tick in y_ticks)
			{
				str = tick.second.c_str();
				tbounds = dc.GetTextExtent(str);
				x_offset = max(x_offset, tbounds.cx);
			}

			for each (auto tick in x_ticks)
			{
				str = tick.second.c_str();
				tbounds = dc.GetTextExtent(str);
				y_offset = max(y_offset, tbounds.cy);
			}

            new_screen.xmin += x_offset + 5;
			new_screen.ymax -= y_offset + 5;

			viewport new_bounds(new_screen, bounds.world);

			dc.MoveTo(new_screen.xmin, 0);
			dc.LineTo(new_screen.xmin, new_screen.ymax);
			dc.LineTo(new_screen.xmax, new_screen.ymax);
			for each (auto tick in x_ticks)
			{
				int x = new_bounds.world_to_screen().x(tick.first);
				str = tick.second.c_str();
				tbounds = dc.GetTextExtent(str);
				dc.MoveTo(x, new_screen.ymin);
				dc.LineTo(x, new_screen.ymax);
				dc.TextOut(x - tbounds.cx / 2, bounds.screen.ymax - y_offset - 2, str);
			}
			for each (auto tick in y_ticks)
			{
				int y = new_bounds.world_to_screen().y(tick.first);
				str = tick.second.c_str();
				tbounds = dc.GetTextExtent(str);
				dc.MoveTo(new_screen.xmin, y);
				dc.LineTo(new_screen.xmax, y);
				dc.TextOut(x_offset - tbounds.cx - 2, y - tbounds.cy / 2, str);
			}

            //str = _T("X");
            //tbounds = dc.GetTextExtent(str);
            //dc.TextOut(new_bounds.screen.xmax - tbounds.cx, bounds.screen.ymax - y_offset - 2, str);
            //str = _T("Y");
            //tbounds = dc.GetTextExtent(str);
            //dc.TextOut(x_offset - tbounds.cx - 2, new_bounds.screen.ymin, str);


            RECT clip = new_screen;
            dc.IntersectClipRect(&clip);
            insides->draw(dc, new_bounds);
		}
	};









	// plot_builder







	class plot_builder
	{

		using layer_ptr		 = layer_drawable::layer_ptr;
        using data_t         = list_drawable::data_t;
        using data_factory_t = list_drawable::data_factory_t;
		using func_t		 = function_drawable::func_t;
		using pen_ptr		 = function_drawable::pen_ptr;
        using painter_t      = custom_drawable::painter_t;
        using viewport_mapper = re_viewporter::viewport_mapper;
		using data_painter_t = list_drawable::painter_t;
		using tf_ptr		 = std::shared_ptr < tick_factory > ;
		using this_t		 = plot_builder &;

		std::unique_ptr<layer_drawable> layers;
		viewport_mapper                 world;
		tf_ptr							x_ticks;
		tf_ptr							y_ticks;
		bool							ticks;
		pen_ptr						    ticks_pen;

		bool							closed;

	public:

		plot_builder()
			: closed(false)
			, ticks(false)
			, layers(std::make_unique<layer_drawable>())
		{
		}

		this_t in_world(const world_t &world)
        {
            this->world = re_world(world);
            return *this;
        }

        this_t in_world(const world_t *world)
        {
            this->world = [world](const viewport &v) { return std::make_shared<viewport>(v.screen, *world); };
            return *this;
        }

		this_t with_ticks(pen_ptr &&pen)
		{
			this->ticks = true;
			this->ticks_pen = std::move(pen);
			return *this;
		}

		this_t with_ticks(COLORREF color = 0x000000)
		{
			this->ticks = true;
			this->ticks_pen = palette::pen(color);
			return *this;
		}

		this_t with_x_ticks(double origin, double interval, int precision = 6)
		{
			x_ticks = std::make_shared<const_interval_tick_factory<axe::x>>(
				plot::make_formatter(precision), origin, interval
			);
			return *this;
		}

		this_t with_y_ticks(double origin, double interval, int precision = 6)
		{
			y_ticks = std::make_shared<const_interval_tick_factory<axe::y>>(
				plot::make_formatter(precision), origin, interval
			);
			return *this;
        }

        this_t with_x_ticks(double origin, int n_intervals, int precision = 6)
        {
            x_ticks = std::make_shared<const_n_tick_factory<axe::x>>(
                plot::make_formatter(precision), origin, n_intervals
            );
            return *this;
        }

        this_t with_y_ticks(double origin, int n_intervals, int precision = 6)
        {
            y_ticks = std::make_shared<const_n_tick_factory<axe::y>>(
                plot::make_formatter(precision), origin, n_intervals
            );
            return *this;
        }

		this_t with_function(func_t f, pen_ptr &&pen)
		{
			layers->with(new function_drawable(std::move(f), std::move(pen)));
			return *this;
		}

		this_t with_function(func_t f, COLORREF color = 0x000000)
		{
			return with_function(std::move(f), palette::pen(color));
		}

		this_t with_data(std::shared_ptr<data_t> data, data_painter_t painter = list_drawable::circle_painter(), palette::pen_ptr &&line_pen = {})
		{
			layers->with(new list_drawable(list_drawable::const_data_factory(data), std::move(painter), std::move(line_pen)));
			return *this;
        }

        this_t with_data(data_factory_t data, data_painter_t painter = list_drawable::circle_painter(), palette::pen_ptr &&line_pen = {})
        {
            layers->with(new list_drawable(std::move(data), std::move(painter), std::move(line_pen)));
            return *this;
        }

		this_t with_layer(layer_ptr &&layer)
		{
			layers->with(std::move(layer));
			return *this;
		}

		this_t with_custom(painter_t painter)
		{
			layers->with(new custom_drawable(std::move(painter)));
			return *this;
		}

		layer_ptr build()
		{
			assert(!closed);

			closed = true;

			if (ticks)
			{
				layer_ptr ticks_layer = std::make_unique<tick_drawable>(
					std::move(layers), std::move(x_ticks), std::move(y_ticks), std::move(ticks_pen)
				);

				return re_world_layer(std::move(ticks_layer));
			}
			else
			{
				return re_world_layer(std::move(layers));
			}
		}

	private:

		layer_ptr re_world_layer(layer_ptr &&layer)
		{
			if (this->world)
			{
				layer_ptr re_viewported = std::make_unique<re_viewporter>(
					std::move(layer), this->world
				);
				return re_viewported;
			}
			return std::move(layer);
		}
	};

	inline std::shared_ptr<list_drawable::data_t> make_data(
        function_drawable::func_t f,
        double a, double b, double interval)
	{
		auto data = std::make_shared<list_drawable::data_t>();
		for (double i = a; i < b; i += interval)
		{
			data->emplace_back(i, f(i));
		}
		return std::move(data);
    }

    inline std::shared_ptr<list_drawable::data_t> make_data(
        function_drawable::func_t f,
        const world_t & world, double interval)
    {
        return make_data(f, world.xmin, world.xmax, interval);
    }
}