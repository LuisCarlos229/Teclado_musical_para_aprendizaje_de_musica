function metricas = calcular_metricas_audio(audio_orig, audio_sint, fs, f0_real, config)
% CALCULAR_METRICAS_AUDIO Extrae MSE, Coherencia Espectral, LSD y Factor de Cresta.
% Versión Corregida: Normalización de amplitud, alineación temporal y 
% cálculo matemático estricto sin "placeholders".

    % 1. NORMALIZACIÓN DE AMPLITUD (Evita penalizaciones por diferencia de volumen)
    audio_orig = audio_orig / max(abs(audio_orig));
    audio_sint = audio_sint / max(abs(audio_sint));

    % 2. ALINEACIÓN TEMPORAL (Cross-correlación)
    % Asegura que ambas señales arranquen exactamente en la misma muestra para 
    % que la resta de espectrogramas no se dispare por desfasajes de milisegundos.
    [acor, lag] = xcorr(audio_orig, audio_sint);
    [~, I] = max(abs(acor));
    lagDiff = lag(I);
    if lagDiff > 0
        audio_sint = [zeros(lagDiff, 1); audio_sint(1:end-lagDiff)];
    elseif lagDiff < 0
        audio_sint = audio_sint(-lagDiff+1:end);
        audio_sint = [audio_sint; zeros(-lagDiff, 1)];
    end

    % 3. CONFIGURACIÓN DE RESOLUCIÓN FFT
    switch config.nivel_resolucion
        case 1, nfft = 512; case 2, nfft = 1024; case 3, nfft = 2048;
        case 4, nfft = 4096; case 5, nfft = 8192; otherwise, nfft = 1024;
    end

    ventana = blackman(nfft);
    solapamiento = round(nfft * 0.90);
    f_limite = (config.N_ARMONICOS_MAX_TABLA + 1.5) * f0_real;

    % 4. CÁLCULO DE ESPECTROGRAMAS
    [S, f_spec, ~] = specgram(audio_orig, nfft, fs, ventana, solapamiento);
    [S_r, f_spec_r, ~] = specgram(audio_sint, nfft, fs, ventana, solapamiento);

    idx_f = find(f_spec <= f_limite);
    
    M1 = abs(S(idx_f, :));
    M2 = abs(S_r(idx_f, :));

    % Igualar cantidad de frames temporales (por si la alineación alteró el largo)
    min_frames = min(size(M1, 2), size(M2, 2));
    M1 = M1(:, 1:min_frames);
    M2 = M2(:, 1:min_frames);

    % 5. MÉTRICAS GLOBALES (MSE, Coherencia, LSD)
    MSE_G = mean((M1(:) - M2(:)).^2);
    
    % SC_G: Verdadera Coherencia Espectral (Similitud Coseno: Rango 0 a 1)
    SC_G = dot(M1(:), M2(:)) / (norm(M1(:)) * norm(M2(:))); 
    
    epsilon = 1e-10;
    LSD_G = mean(sqrt(mean((10*log10(M1+epsilon) - 10*log10(M2+epsilon)).^2, 1)));

    % 6. MÉTRICAS DE BANDA ARMÓNICA (Extracción real, sin constantes inventadas)
    % Se crea una máscara que solo deja pasar la energía ubicada sobre los armónicos
    mascara = zeros(length(idx_f), 1);
    tolerancia_hz = f0_real * 0.08; % 8% de margen alrededor de cada armónico
    
    for k = 1:config.N_ARMONICOS_MAX_TABLA
        f_armonico = k * f0_real;
        idx_arm = abs(f_spec(idx_f) - f_armonico) < tolerancia_hz;
        mascara(idx_arm) = 1;
    end

    idx_validos = find(mascara == 1);
    M1_B = M1(idx_validos, :);
    M2_B = M2(idx_validos, :);

    % Coherencia y LSD evaluadas EXCLUSIVAMENTE sobre las frecuencias armónicas
    SC_B = dot(M1_B(:), M2_B(:)) / (norm(M1_B(:)) * norm(M2_B(:)));
    LSD_B = mean(sqrt(mean((10*log10(M1_B+epsilon) - 10*log10(M2_B+epsilon)).^2, 1)));

    % 7. DIFERENCIA DE FACTOR DE CRESTA (CFR)
    % El audio ya está normalizado, pero RMS_orig debe recalcularse.
    RMS_orig = sqrt(mean(audio_orig.^2)); CF_orig = 1 / RMS_orig;
    RMS_sint = sqrt(mean(audio_sint.^2)); CF_sint = 1 / RMS_sint;
    Dif_CFR = abs(CF_orig - CF_sint);

    % 8. EMPAQUETADO (Mismo formato de salida para mantener compatibilidad con el reporte)
    metricas = [MSE_G, SC_G, LSD_G, SC_B, LSD_B, Dif_CFR];
end