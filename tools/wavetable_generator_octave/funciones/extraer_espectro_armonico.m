function [amplitudes, num_armonicos] = extraer_espectro_armonico(canal_analisis, fs, f0_real, config)
% EXTRAER_ESPECTRO_ARMONICO (Versión con régimen estable común)
% Obtiene la magnitud de los armónicos de una señal con interpolación
% parabólica (mitiga el "scalloping loss"), respetando Nyquist y aplicando
% un umbral de ruido. La ventana se ubica con localizar_regimen_estable, el
% mismo criterio que usa detectar_fundamental, y la búsqueda de cada parcial
% sigue el espaciado medido (tolera inarmonicidad moderada: piano, láminas).

    % 1. Régimen estable (criterio común con detectar_fundamental)
    [idx_ini, longitud_ventana] = localizar_regimen_estable(canal_analisis, fs, f0_real, config);
    ventana_analisis = canal_analisis(idx_ini : idx_ini + longitud_ventana - 1);
    ventana_analisis = ventana_analisis(:) - mean(ventana_analisis);   % elimina DC y pops de baja frecuencia

    % 2. Enventanado y FFT con zero-padding (mejor localización de picos)
    senal_enventanada = ventana_analisis .* hanning(longitud_ventana);
    N_fft = 2^nextpow2(longitud_ventana * 4);
    Y = fft(senal_enventanada, N_fft);
    Magnitud_single = abs(Y(1:floor(N_fft/2)+1)) / longitud_ventana;
    Magnitud_single(2:end-1) = 2 * Magnitud_single(2:end-1);
    n_bins = length(Magnitud_single);
    hz_por_bin = fs / N_fft;

    % Margen mínimo de búsqueda: 2 bines de la resolución real (sin padding)
    margen_min_idx = ceil(2 * N_fft / longitud_ventana);

    % Límite superior de búsqueda: el menor entre el Nyquist del archivo original
    % y el del ESP32 (FS_DESTINO), descontando la transposición máxima con que una
    % tecla puede reutilizar esta tabla. Así ningún armónico de la tabla se refleja
    % (aliasing) al reproducirse en el sintetizador.
    f_limite = fs/2 * config.MARGEN_NYQUIST;
    if isfield(config, 'FS_DESTINO') && config.FS_DESTINO > 0
        transp = 0;
        if isfield(config, 'MAX_TRANSPOSICION_ST'), transp = config.MAX_TRANSPOSICION_ST; end
        margen_dest = 0.90;
        if isfield(config, 'MARGEN_NYQUIST_DESTINO'), margen_dest = config.MARGEN_NYQUIST_DESTINO; end
        f_limite = min(f_limite, config.FS_DESTINO/2 * margen_dest / 2^(transp/12));
    end

    % 3. Búsqueda de parciales con seguimiento del espaciado
    % El centro de búsqueda de cada parcial se extrapola linealmente a partir de los dos
    % últimos parciales confiables (sigue la inarmonicidad y tolera parciales ausentes).
    piso_ruido = median(Magnitud_single);   % referencia del piso espectral
    amplitudes = zeros(config.N_ARMONICOS_MAX_TABLA, 1);
    k_a = 0; f_a = 0;                       % penúltimo parcial confiable (índice, frecuencia medida)
    k_b = 0; f_b = 0;                       % último parcial confiable
    pico_max = 0;
    k = 1;

    while k <= config.N_ARMONICOS_MAX_TABLA
        if k_a > 0
            f_centro = f_b + (f_b - f_a) * (k - k_b) / (k_b - k_a);   % extrapolación del espaciado medido
        else
            f_centro = k * f0_real;                                    % posición armónica nominal
        end

        if f_centro >= f_limite
            break;
        end

        idx_teorico = round(f_centro / hz_por_bin) + 1;
        margen_idx  = max(margen_min_idx, round(f_centro * config.MARGEN_BUSQUEDA_ARMONICO_PCT / hz_por_bin));
        idx_min = max(1, idx_teorico - margen_idx);
        idx_max = min(n_bins, idx_teorico + margen_idx);

        if idx_min >= n_bins
            break;
        end

        [max_val, idx_pico_local] = max(Magnitud_single(idx_min:idx_max));
        idx_pico = idx_min + idx_pico_local - 1;

        % Corrección sub-bin (interpolación parabólica)
        p = 0;
        A_k = max_val;
        if idx_pico > 1 && idx_pico < n_bins
            alpha = Magnitud_single(idx_pico - 1);
            beta  = Magnitud_single(idx_pico);
            gamma = Magnitud_single(idx_pico + 1);
            denom = (alpha - 2*beta + gamma);
            if denom ~= 0
                p = 0.5 * (alpha - gamma) / denom;
                if abs(p) > 0.5, p = 0; end
                A_k = beta - 0.25 * (alpha - gamma) * p;
            else
                A_k = beta;
            end
        end
        f_medida = (idx_pico - 1 + p) * hz_por_bin;

        amplitudes(k) = A_k;
        pico_max = max(pico_max, A_k);

        % Un parcial es confiable si sobresale 20 dB del piso, supera el umbral del noise gate
        % y su frecuencia es físicamente razonable (cuerdas rígidas: f_k >= k*f0)
        confiable = (A_k > 10 * piso_ruido) && (A_k >= pico_max * 10^(config.UMBRAL_ARMONICO_dB / 20)) ...
                    && (f_medida >= 0.99 * k * f0_real) && (f_medida <= 1.5 * k * f0_real);
        if confiable
            k_a = k_b; f_a = f_b;
            k_b = k;   f_b = f_medida;
        end

        k = k + 1;
    end

    num_armonicos = k - 1;
    amplitudes = amplitudes(1:num_armonicos);

    % 4. "Noise Gate" (Umbral de Armónico)
    if num_armonicos > 0
        umbral_lineal = max(amplitudes) * 10^(config.UMBRAL_ARMONICO_dB / 20);
        amplitudes(amplitudes < umbral_lineal) = 0;
    end
end