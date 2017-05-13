#pragma once

#include <cstdint>

namespace act_photo
{

    const std::uint8_t method_get = 0;
    const std::uint8_t method_set = 1;

    const std::uint8_t var_packet = 7;
    const std::uint8_t var_coefs  = 5;

    const std::uint8_t packet_size      = 10;
    const std::uint8_t packet_body_size = 9;
    const std::uint8_t packet_delimiter = 0;

    using packet_t = struct
    {
        std::uint8_t adc1, adc2;
        std::int16_t cur_err, int_err, pwm;
        std::uint8_t ocr2;
    };

    using desired_coefs_t = struct
    {
        double kp, ki, ks;
    };

    using coefs_t = struct
    {
        std::uint16_t kp_m; std::uint8_t kp_d;
        std::uint16_t ki_m; std::uint8_t ki_d;
        std::uint16_t ks_m; std::uint8_t ks_d;
    };

    using command_t = struct
    {
        using data_t = std::vector < std::uint8_t > ;

        std::uint8_t method;
        std::uint8_t var;
        data_t       bytes;
    };

    inline packet_t read_packet(char * input /* without headings */)
    {
        return {
            (std::uint8_t) ((std::uint8_t) input[0]),
            (std::uint8_t) ((std::uint8_t) input[1]),
            (std::int16_t) ((((std::uint8_t) input[2]) << 8) | ((std::uint8_t) input[3])),
            (std::int16_t) ((((std::uint8_t) input[4]) << 8) | ((std::uint8_t) input[5])),
            (std::int16_t) ((((std::uint8_t) input[6]) << 8) | ((std::uint8_t) input[7])),
            (std::uint8_t) ((std::uint8_t) input[8])
        };
    }

    inline void serialize(const command_t &command, char * output)
    {
        output[0] = command.method;
        output[1] = command.var;
        memcpy_s(output,               command.bytes.size() + 2,
                 command.bytes.data(), command.bytes.size() + 2);
    }

    inline void calculate_optimal_coef
        (
        double desired, double &optimal, std::uint16_t &base, std::uint8_t &exponent
        )
    {
        double x0 = (int) desired, x;
        std::uint16_t y, y0 = x0;
        std::uint8_t i0 = 0;
        for (std::uint8_t i = 1; i < 15; i++)
        {
            y = (int) ((1 << i) * desired);
            x = ((double) y) / (1 << i);
            if ((abs(x - desired) < abs(x0 - desired))
                && (y < (std::numeric_limits<std::uint16_t>::max)() / 2))
            {
                x0 = x;
                y0 = y;
                i0 = i;
            }
        }
        optimal = x0;
        base = y0;
        exponent = i0;
    }

    inline coefs_t calculate_optimal_coefs(desired_coefs_t desired,
                                           desired_coefs_t &optimal)
    {
        coefs_t coefs;
        calculate_optimal_coef(desired.kp, optimal.kp, coefs.kp_m, coefs.kp_d);
        calculate_optimal_coef(desired.ki, optimal.ki, coefs.ki_m, coefs.ki_d);
        calculate_optimal_coef(desired.ks, optimal.ks, coefs.ks_m, coefs.ks_d);
        return coefs;
    }

    inline command_t set_coefs_command(coefs_t coefs)
    {
        command_t::data_t bytes(9);
        bytes[0] = (coefs.kp_m >> 8) & 0xff; bytes[1] = (coefs.kp_m & 0xff);
        bytes[2] = coefs.kp_d;
        bytes[3] = (coefs.ki_m >> 8) & 0xff; bytes[4] = (coefs.ki_m & 0xff);
        bytes[5] = coefs.ki_d;
        bytes[6] = (coefs.ks_m >> 8) & 0xff; bytes[7] = (coefs.ks_m & 0xff);
        bytes[8] = coefs.ks_d;
        return { method_set, var_coefs, bytes };
    }

    inline command_t set_coefs_command(desired_coefs_t coefs)
    {
        desired_coefs_t optimal;
        return set_coefs_command(calculate_optimal_coefs(coefs, optimal));
    }

    inline command_t get_packet_command()
    {
        return { method_get, var_packet, command_t::data_t() };
    }
}