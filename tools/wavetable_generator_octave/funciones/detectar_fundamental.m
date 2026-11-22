function f0_real = detectar_fundamental(canal_analisis, fs, f0_teorica, config)
% DETECTAR_FUNDAMENTAL (Versión Corregida)
% Aísla el régimen estable de la señal e implementa Zero-Padding masivo
% combinado con interpolación parabólica y tolerancia dinámica para
% sortear la inarmonía y garantizar precisión sub-Hz en todas las octavas.

    % 1. Régimen estable (criterio común con extraer_espectro_armonico)
    [idx_regimen_estable, longitud_ventana] = localizar_regimen_estable(canal_analisis, fs, f0_teorica, config);
    ventana_analisis = canal_analisis(idx_regimen_estable : idx_regimen_estable + longitud_ventana - 1);
    ventana_analisis = ventana_analisis(:) - mean(ventana_analisis);   % elimina DC y pops de baja frecuencia

    % 4. Enventanado (Hanning)
    ventana_hann = hanning(longitud_ventana);
    senal_enventanada = ventana_analisis .* ventana_hann;

    % 5. Zero-Padding (Interpolación Espectral)
    % Se rellena con ceros hasta alcanzar 4 veces la siguiente potencia de 2.
    % Esto asegura que incluso para ventanas cortas (notas agudas),
    % la cuadrícula de frecuencias (Delta f) sea menor a 5 Hz antes de la parábola.
    L_fft = length(senal_enventanada);
    N_pad = 2^nextpow2(L_fft * 4);

    Y = fft(senal_enventanada, N_pad);

    Magnitud_single = abs(Y(1:floor(N_pad/2)+1)) / L_fft;
    f_axis_fft = fs * (0:floor(N_pad/2)) / N_pad;

    % 6. Tolerancia Dinámica por Inarmonía de Pianos
    % Se ignora config.TOLERANCIA_BUSQUEDA_F0 para usar un perfil adaptativo.
    if f0_teorica < 250
        tolerancia = 0.05; % Graves: 5% (armónicos juntos, requiere precisión)
    elseif f0_teorica < 1000
        tolerancia = 0.10; % Medios: 10%
    else
        tolerancia = 0.20; % Agudos (C8): 20% (cuerdas muy tensas/rígidas)
    end

    f_inf_busqueda = f0_teorica * (1 - tolerancia);
    f_sup_busqueda = f0_teorica * (1 + tolerancia);

    idx_banda = find(f_axis_fft >= f_inf_busqueda & f_axis_fft <= f_sup_busqueda);

    if isempty(idx_banda)
        f0_real = f0_teorica;
        warning('detectar_fundamental: sin datos en la banda de búsqueda, se usa la frecuencia teórica.');
    else
        % Búsqueda del pico acotada a la banda
        [~, idx_pico_local] = max(Magnitud_single(idx_banda));
        idx_pico = idx_banda(idx_pico_local);

        % 7. Interpolación Parabólica para corrección Sub-bin final
        if idx_pico > 1 && idx_pico < length(Magnitud_single)
            alpha = Magnitud_single(idx_pico - 1);
            beta  = Magnitud_single(idx_pico);
            gamma = Magnitud_single(idx_pico + 1);

            denom = (alpha - 2*beta + gamma);
            p = 0;
            if denom ~= 0
                p = 0.5 * (alpha - gamma) / denom;
            end

            % Evita desvíos matemáticos limitando el ajuste a +/- 0.5 bines
            if abs(p) > 0.5
                p = 0;
            end
        else
            p = 0;
        end

        % Frecuencia final matemáticamente exacta
        f0_real = f_axis_fft(idx_pico) + p * (fs / N_pad);
    end

    % Impresión del resultado
    fprintf('---> [DETECCIÓN] Frec. Teórica: %8.3f Hz | Frec. Real: %8.3f Hz\n', f0_teorica, f0_real);

end