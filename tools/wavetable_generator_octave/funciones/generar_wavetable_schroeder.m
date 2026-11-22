function wavetable_sintetizada = generar_wavetable_schroeder(amplitudes, num_armonicos, N_WAVETABLE)
% GENERAR_WAVETABLE_SCHROEDER Sintetiza una forma de onda (Wavetable)
% sumando armónicos e implementando fases de Schroeder para minimizar
% el Factor de Cresta (CFR).
%
%   Uso: wavetable = generar_wavetable_schroeder(amplitudes, num_armonicos, N_WAVETABLE)
%
%   Entradas:
%       amplitudes    - Vector con las magnitudes lineales de cada armónico.
%       num_armonicos - Cantidad total de armónicos.
%       N_WAVETABLE   - Tamaño de la tabla (ej. 1024 o 2048).
%   Salida:
%       wavetable_sintetizada - Vector columna normalizado de la forma de onda.

    % 1. Inicialización del vector de tiempo normalizado (0 a 1)
    t_table = (0:N_WAVETABLE-1)' / N_WAVETABLE;

    % 2. Inicialización del buffer de la wavetable
    wavetable_sintetizada = zeros(N_WAVETABLE, 1);

    % 3. Síntesis Aditiva con dispersión de fase
    for k = 1:num_armonicos
        if amplitudes(k) > 0
            % Cálculo de la fase de Schroeder para el armónico k
            fase_schroeder = - (pi * k^2) / num_armonicos;

            % Suma del armónico a la tabla principal
            wavetable_sintetizada = wavetable_sintetizada + ...
                amplitudes(k) * cos(2 * pi * k * t_table + fase_schroeder);
        end
    end

    % 4. Normalización de Amplitud (-1.0 a 1.0)
    % Fundamental para aprovechar el rango dinámico máximo del DAC del ESP32
    max_val = max(abs(wavetable_sintetizada));
    if max_val > 0
        wavetable_sintetizada = wavetable_sintetizada / max_val;
    end

end
