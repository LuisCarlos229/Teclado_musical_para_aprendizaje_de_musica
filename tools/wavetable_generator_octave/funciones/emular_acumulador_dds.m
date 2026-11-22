function audio_sintetizado = emular_acumulador_dds(wavetable, envolvente, fs, f0_real, audio_original, config)
% EMULAR_ACUMULADOR_DDS Simula matemáticamente el comportamiento del
% acumulador de fase (punto fijo) que tendrá el microcontrolador (ESP32),
% con la misma cuantización e interpolación de envolvente que el firmware.

    muestras_totales = round(config.duracion_reconstruccion * fs);
    muestras_originales = length(audio_original);

    % Corrección: Calcular coeficientes separados antes de filtrar
    [b_vol, a_vol] = butter(2, [max(1, f0_real-10), min(fs/2-1, f0_real+10)]/(fs/2), 'bandpass');
    volumen_maximo = max(abs(filtfilt(b_vol, a_vol, audio_original)));

    % Cuantización igual a la del .h exportado (int16 la tabla, uint16 la envolvente),
    % para que las métricas evalúen exactamente lo que recibe el ESP32
    wavetable  = round(wavetable(:) * 32767) / 32767;
    envolvente = min(max(round(envolvente(:) * 65535), 0), 65535) / 65535;

    % Expansión de la envolvente al tiempo real, con interpolación LINEAL como en
    % Task_audio.h. Contrato con el firmware: el punto j (0..N-1) de la envolvente
    % corresponde al instante j*duracion/(N-1), es decir, la envolvente completa se
    % recorre en duracion_reconstruccion segundos (el valor exportado a DURACION_S).
    t_puntos_env = linspace(0, 1, config.N_puntos_envolvente);
    t_real       = linspace(0, 1, muestras_originales);
    env_simulada = interp1(t_puntos_env, envolvente, t_real, 'linear')(:) * volumen_maximo;

    if muestras_totales > muestras_originales
        envolvente_estirada = [env_simulada; zeros(muestras_totales - muestras_originales, 1)];
    else
        envolvente_estirada = env_simulada(1:muestras_totales);
    end

    % Parámetros DDS
    BITS_INDICE = log2(config.N_WAVETABLE);
    BITS_FRACCION = config.BITS_ACUMULADOR_DDS - BITS_INDICE;
    MODULO_ACUMULADOR = 2^config.BITS_ACUMULADOR_DDS;
    FACTOR_FRACCION = 2^BITS_FRACCION;

    % Tuning Word para f0_real
    tuning_word = round(f0_real * MODULO_ACUMULADOR / fs);

    % Lazo del Acumulador Vectorizado
    n_muestra = (0:muestras_totales-1)';
    acumulador = mod(n_muestra * tuning_word, MODULO_ACUMULADOR);

    indice0 = floor(acumulador / FACTOR_FRACCION);
    fraccional = (acumulador - indice0 * FACTOR_FRACCION) / FACTOR_FRACCION;

    idx1 = indice0 + 1;
    idx2 = mod(idx1, config.N_WAVETABLE) + 1;

    % Interpolación Lineal de la tabla
    onda_continua = wavetable(idx1) + fraccional .* (wavetable(idx2) - wavetable(idx1));

    % Modulación de amplitud final
    audio_sintetizado = onda_continua .* envolvente_estirada;
end
