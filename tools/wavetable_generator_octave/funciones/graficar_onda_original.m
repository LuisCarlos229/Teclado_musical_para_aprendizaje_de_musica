function graficar_onda_original(canal_analisis, nombre_base, dir_notas)
% GRAFICAR_ONDA_ORIGINAL Genera y guarda la gráfica 2D de la onda de audio.

    carpeta_onda = fullfile(dir_notas, 'graficos_onda');
    if ~exist(carpeta_onda, 'dir'), mkdir(carpeta_onda); end

    fig_onda = figure('visible', 'off');
    plot(canal_analisis); grid on;
    title(['Forma de onda original: ', nombre_base]);
    xlabel('Índice de la muestra'); ylabel('Amplitud');

    ruta_onda = fullfile(carpeta_onda, [nombre_base, '_Onda_Original.png']);
    print(fig_onda, ruta_onda, '-dpng', '-r200');
    close(fig_onda);
end
