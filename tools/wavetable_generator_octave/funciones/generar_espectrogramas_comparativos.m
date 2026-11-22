function generar_espectrogramas_comparativos(audio_orig, audio_sint, fs, f0_real, nombre, dir_notas, config)
% GENERAR_ESPECTROGRAMAS_COMPARATIVOS Exporta las vistas Waterfall 3D.

    carpeta_img = fullfile(dir_notas, 'espectrogramas');
    if ~exist(carpeta_img, 'dir'), mkdir(carpeta_img); end

    nfft = 4096; ventana = blackman(nfft); solapamiento = round(nfft * 0.90);
    f_limite = (config.N_ARMONICOS_MAX_TABLA + 1.5) * f0_real;

    % ORIGINAL
    [S, f_spec, ~] = specgram(audio_orig, nfft, fs, ventana, solapamiento);
    idx_f = find(f_spec <= f_limite);
    M1 = abs(S(idx_f, :));

    fig_orig = figure('visible', 'off');
    waterfall(f_spec(idx_f), linspace(0, config.duracion_reconstruccion, size(M1,2)), M1');
    view(145, 45); colormap(jet);
    title(['ORIGINAL - ', nombre]);
    ylim([0, config.duracion_reconstruccion]);
    xlabel('Frecuencia (Hz)'); ylabel('Tiempo (s)'); zlabel('Amplitud');
    print(fig_orig, fullfile(carpeta_img, [nombre, '_1_Original.png']), '-dpng', '-r200');
    close(fig_orig);

    % SINTESIS
    [S_r, f_spec_r, ~] = specgram(audio_sint, nfft, fs, ventana, solapamiento);
    idx_f_r = find(f_spec_r <= f_limite);
    M2 = abs(S_r(idx_f_r, :));

    fig_sint = figure('visible', 'off');
    waterfall(f_spec_r(idx_f_r), linspace(0, config.duracion_reconstruccion, size(M2,2)), M2');
    view(145, 45); colormap(jet);
    title(['SÍNTESIS - ', nombre]);
    ylim([0, config.duracion_reconstruccion]);
    xlabel('Frecuencia (Hz)'); ylabel('Tiempo (s)'); zlabel('Amplitud');
    print(fig_sint, fullfile(carpeta_img, [nombre, '_2_Sintesis.png']), '-dpng', '-r200');
    close(fig_sint);
end
