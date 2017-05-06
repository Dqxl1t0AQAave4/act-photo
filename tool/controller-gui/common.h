#pragma once

#include <functional>

#include "plot.h"

#ifndef M_PI
    #define M_PI 3.141592653589793238462643
#endif

namespace common
{



    // type aliases



    using continuous_t = std::function < double(double) > ;
    using sampled_t    = struct
                         {
                             double *samples;
                             size_t count;
                             double period;
                         };
    using bi_op_t      = std::function < double(size_t, double, double) > ;
    using un_op_t      = std::function < double(size_t, double) > ;

    using data_t       = plot::list_drawable::data_t;
    using data_ptr     = std::shared_ptr < data_t > ;
    using pair_t       = std::pair<double, double>;
    using plot::world_t;

    using gaussian_t = struct
    {
        double a, s, t0;
    };



    // operators



    inline bi_op_t identity_op()
    {
        return [] (size_t, double, double s) { return s; };
    }

    inline un_op_t identity_un_op()
    {
        return [] (size_t, double s) { return s; };
    }

    inline bi_op_t add_op()
    {
        return [] (size_t, double d, double s) { return d + s; };
    }

    inline bi_op_t mult_add_op(double factor)
    {
        return [factor] (size_t, double d, double s) { return d + s * factor; };
    }



    // signals




    inline double random(double left = -1, double right = 1)
    {
        return ((double) rand()) / RAND_MAX * (right - left) + left;
    }

    inline continuous_t sin(double magnitude, double frequency)
    {
        return [=] (double t) { return magnitude * std::sin(2 * M_PI * frequency * t); };
    }

    inline continuous_t gaussian(gaussian_t params)
    {
        return [=] (double t) { return params.a * std::exp(- (t - params.t0)*(t - params.t0) / params.s/params.s); };
    }

    inline continuous_t noise(double left = -1, double right = 1, size_t trust = 20)
    {
        return [=] (double t)
        {
            double noise = 0;
            for (size_t i = 0; i < trust; i++)
            {
                noise += random(left, right);
            }
            return noise / trust;
        };
    }

    inline continuous_t combine(size_t n, continuous_t *funcs, bi_op_t combiner = add_op())
    {
        return [=] (double t)
        {
            continuous_t &first = funcs[0];
            double result = first(t);
            for (size_t i = 1u; i < n; i++)
            {
                result = combiner(i, result, funcs[i](t));
            }
            return result;
        };
    }



    // samplers




    /**
     * Returns the signal power.
     */
    inline double sample(continuous_t &continuous,
                         sampled_t    &sampled,
                         size_t       samples = 0)
    {
        if (samples == 0) samples = sampled.count;
        assert(samples <= sampled.count);
        double power = 0, val;
        for (size_t i = 0; i < samples; i++)
        {
            val = continuous(i * sampled.period);
            sampled.samples[i] = val;
            power += val * val;
        }
        return power;
    }

    inline sampled_t allocate_sampled(size_t size, size_t count, double period)
    {
        return { new double[size], count, period };
    }

    inline sampled_t allocate_sampled(size_t size, double period)
    {
        return { new double[size], size, period };
    }

    inline void free_sampled(sampled_t sampled)
    {
        delete[] sampled.samples;
    }



    
    // mappers




    inline void map(sampled_t &dest,
                    sampled_t &source,
                    bi_op_t      op = identity_op())
    {
        assert(dest.count == source.count);
        assert(abs(dest.period - source.period) < 1e-15);
        for (size_t i = 0; i < dest.count; i++)
        {
            dest.samples[i] = op(i, dest.samples[i], source.samples[i]);
        }
    }

    inline void map(sampled_t &dest,
                    un_op_t   op)
    {
        for (size_t i = 0; i < dest.count; i++)
        {
            dest.samples[i] = op(i, dest.samples[i]);
        }
    }



    // special functions



    inline void autocorrelation(sampled_t &input,
                                sampled_t &output)
    {
        assert(input.count > output.count);
        assert(abs(input.period - output.period) < 1e-15);
        size_t n = input.count, m = output.count;
        for (size_t k = 0; k < m; k++)
        {
            output.samples[k] = 0;
            for (size_t i = 0; i < n - k; i++)
            {
                output.samples[k] += input.samples[i] * input.samples[i + k];
            }
            output.samples[k] /= (n - k);
        }
    }

    inline std::pair<size_t, double> correlation(
        sampled_t &input1,
        sampled_t &input2,
        sampled_t &output,
        bool reflect = false)
    {
        assert(input1.count == input2.count);
        assert(input1.count >= output.count);
        assert(abs(input1.period - input2.period) < 1e-15);
        assert(abs(input1.period - output.period) < 1e-15);
        size_t n = input1.count, m = output.count;
        double max = (std::numeric_limits<double>::min)(); size_t max_idx = 0;
        for (size_t k = 0; k < m; k++)
        {
            output.samples[k] = 0;
            for (size_t i = 0; i < n; i++)
            {
                size_t idx = reflect ? n - i - 1 : i;
                output.samples[k] += input1.samples[i] * input2.samples[(idx + k) % n];
            }
            output.samples[k] /= (n);
            if (output.samples[k] > max)
            {
                max = output.samples[k];
                max_idx = k;
            }
        }
        return { max_idx, max };
    }



    inline double convolute(sampled_t &f,
                            sampled_t &g,
                            sampled_t &output,
                            un_op_t mapper = identity_un_op())
    {
        assert(f.count == g.count);
        assert(f.count <= output.count);
        assert(abs(f.period - g.period) < 1e-15);
        assert(abs(f.period - output.period) < 1e-15);
        size_t n = f.count;
        double power = 0;
        for (size_t i = 0; i < n; i++)
        {
            output.samples[i] = 0;
            for (size_t j = 0; j < n; j++)
            {
                int conv_idx = ((int) i - (int) j);
                if (conv_idx < 0) conv_idx = (n + conv_idx);
                output.samples[i] += f.samples[j] * g.samples[conv_idx];
            }
            output.samples[i] = mapper(i, output.samples[i]);
            power += output.samples[i] * output.samples[i];
        }
        return power;
    }

    inline double convolute(sampled_t &f,
                            sampled_t &g,
                            size_t i)
    {
        assert(f.count == g.count);
        assert(abs(f.period - g.period) < 1e-15);
        double result = 0;
        for (size_t j = 0; j < f.count; j++)
        {
            int conv_idx = ((int) i - (int) j);
            if (conv_idx < 0) conv_idx = (f.count + conv_idx);
            result += f.samples[j] * g.samples[conv_idx];
        }
        return result;
    }




    // plotting






    struct simple_list_plot
    {
        data_ptr data;
        world_t  world;
        size_t   point_size;
        size_t   line_width;
        COLORREF color;
        bool     connect;
        bool     visible;

        static simple_list_plot curve(COLORREF color    = 0x000000,
                                      size_t line_width = 2)
        {
            return simple_list_plot(0, {}, color, 0, line_width, true);
        }

        static simple_list_plot points(COLORREF color    = 0x000000,
                                       size_t point_size = 2)
        {
            return simple_list_plot(0, {}, color, point_size, 0, false);
        }

        static simple_list_plot connected_points(COLORREF color    = 0x000000,
                                                 size_t point_size = 2,
                                                 size_t line_width = 1)
        {
            return simple_list_plot(0, {}, color, point_size, line_width, true);
        }

        simple_list_plot(size_t   initial_size = 0,
                         world_t  world        = {},
                         COLORREF color        = 0x000000,
                         size_t   point_size   = 2,
                         size_t   line_width   = 1,
                         bool     connect      = true,
                         bool     visible      = true)
            : data(std::make_shared<data_ptr::element_type>(initial_size))
            , world(world)
            , color(color)
            , point_size(point_size)
            , line_width(line_width)
            , connect(connect)
            , visible(visible)
        {
        }
    };

    inline void setup(simple_list_plot &plot,
                      sampled_t        &source,
                      size_t           n_points = (std::numeric_limits<size_t>::max)(),
                      int              shift = 0,
                      un_op_t          op = identity_un_op(),
                      bool             update_world    = true,
                      bool             symmetric_world = false,
                      double           y_factor        = 1.1,
                      int              x_spacing       = 1)
    {
        double ymin = (std::numeric_limits<double>::max)();
        double ymax = (std::numeric_limits<double>::min)();
        n_points = min(n_points, source.count);
        if (plot.visible)
        {
            plot.data->resize(n_points);
        }
        else
        {
            plot.data->clear();
        }
        for (size_t i = 0; i < n_points; i++)
        {
            double y = op(i, source.samples[i]);
            if (plot.visible)
            {
                plot.data->at(i) = { source.period * (int(i) + shift), y };
            }
            if (y < ymin)
            {
                ymin = y;
            }
            if (y > ymax)
            {
                ymax = y;
            }
        }
        if (update_world)
        {
            if (symmetric_world)
            {
                if (abs(ymin) > abs(ymax))
                {
                    ymax = -ymin;
                }
                else
                {
                    ymin = -ymax;
                }
            }
            plot.world.ymin = ymin * ((ymin < 0) ? y_factor : 1 / y_factor);
            plot.world.ymax = ymax * ((ymax > 0) ? y_factor : 1 / y_factor);
            plot.world.xmin = source.period * (shift - x_spacing);
            plot.world.xmax = source.period * (source.count + shift + x_spacing);
        }
    }
    
    inline void setup(simple_list_plot &plot,
                      double           *x,
                      double           *y,
                      size_t           n,
                      bool             update_world   = true,
                      bool             symmetric_y    = false,
                      bool             symmetric_x    = false,
                      bool             both_symmetric = false,
                      double           y_factor       = 1.1,
                      int              x_factor       = 1.1)
    {
        world_t min_max = {
            (std::numeric_limits<double>::max)(), (std::numeric_limits<double>::min)(),
            (std::numeric_limits<double>::max)(), (std::numeric_limits<double>::min)()
        };
        plot.data->resize(n);
        for (size_t i = 0; i < n; i++)
        {
            plot.data->at(i) = { x[i], y[i] };
            if (y[i] < min_max.ymin) { min_max.ymin = y[i]; }
            if (x[i] < min_max.xmin) { min_max.xmin = x[i]; }
            if (y[i] > min_max.ymax) { min_max.ymax = y[i]; }
            if (x[i] > min_max.xmax) { min_max.xmax = x[i]; }
        }
        if (update_world)
        {
            if (symmetric_y)
            {
                if (abs(min_max.ymin) > abs(min_max.ymax)) { min_max.ymax = -min_max.ymin; }
                else                                       { min_max.ymin = -min_max.ymax; }
            }
            if (symmetric_x)
            {
                if (abs(min_max.xmin) > abs(min_max.xmax)) { min_max.xmax = -min_max.xmin; }
                else                                       { min_max.xmin = -min_max.xmax; }
            }
            if (both_symmetric)
            {
                assert(symmetric_x && symmetric_y);
                if (min_max.xmax > min_max.ymax) { min_max = { -min_max.xmax, min_max.xmax, -min_max.xmax, min_max.xmax }; }
                else                             { min_max = { -min_max.ymax, min_max.ymax, -min_max.ymax, min_max.ymax }; }
            }
            plot.world.ymin = min_max.ymin * y_factor;
            plot.world.ymax = min_max.ymax * y_factor;
            plot.world.xmin = min_max.xmin * x_factor;
            plot.world.xmax = min_max.xmax * x_factor;
        }
    }

    inline plot::plot_builder & operator << (plot::plot_builder &builder, simple_list_plot &plot_struct)
    {
        return builder
            .in_world(&plot_struct.world)
            .with_data(plot::list_drawable::const_data_factory(plot_struct.data),
                       plot::list_drawable::circle_painter(plot_struct.point_size, plot::palette::brush(plot_struct.color)),
                       (plot_struct.connect ? plot::palette::pen(plot_struct.color, plot_struct.line_width) : plot::palette::pen_ptr()));
    }




    // utility



    inline void sort_all(double **arrays, size_t n_arrays, size_t n_elements, int order = 1 /* lower */)
    {
        assert(n_arrays > 0);
        double *keys = arrays[0];
        for (size_t i = 0; i < n_elements; i++)
        {
            for (size_t j = i + 1; j < n_elements; j++)
            {
                if (order * (keys[i] - keys[j]) > 0)
                {
                    for (size_t k = 0; k < n_arrays; k++)
                    {
                        std::swap(arrays[k][i], arrays[k][j]);
                    }
                }
            }
        }
    }
}