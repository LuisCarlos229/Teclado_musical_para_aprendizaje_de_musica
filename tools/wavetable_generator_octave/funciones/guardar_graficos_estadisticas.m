function guardar_graficos_estadisticas(nombres, stats_matrix, dir_notas)
% GUARDAR_GRAFICOS_ESTADISTICAS Genera y guarda gráficas semilogarítmicas
% del comportamiento de las métricas en función de la frecuencia.

    carpeta_stats = fullfile(dir_notas, 'estadisticas');
    if ~exist(carpeta_stats, 'dir'), mkdir(carpeta_stats); end

    frecs_fundamentales = stats_matrix(:, 2); % Columna de f0_real
    [frecs_ordenadas, idx_orden] = sort(frecs_fundamentales);
    datos_ordenados = stats_matrix(idx_orden, :);
    nombres_ordenados = nombres(idx_orden);

    metricas_nombre  = {'Diferencia de frecuencia (%)', 'MSE (Global)', 'SC Global', 'LSD Global (dB)', ...
        'SC Banda Armonica', 'LSD Banda Armonica (dB)', 'Diferencia de CFR', 'Duracion exportada (s)'};
    metricas_columna = [4, 5, 6, 7, 8, 9, 10, 11];

    for m = 1:length(metricas_nombre)
        fig_stat = figure('visible', 'off');
        semilogx(frecs_ordenadas, datos_ordenados(:, metricas_columna(m)), '-o', ...
            'LineWidth', 1.5, 'MarkerSize', 6);
        grid on;
        xlabel('Frecuencia fundamental detectada (Hz)');
        ylabel(metricas_nombre{m});
        title(['Comportamiento de ', metricas_nombre{m}]);

        % Agregar etiquetas de las notas en cada punto
        for p = 1:length(frecs_ordenadas)
            text(frecs_ordenadas(p), datos_ordenados(p, metricas_columna(m)), ...
                ['  ', nombres_ordenados{p}], 'FontSize', 8, 'Interpreter', 'none');
        end

        nombre_archivo = ['Metrica_', num2str(m), '_', strrep(metricas_nombre{m}, ' ', '_'), '.png'];
        print(fig_stat, fullfile(carpeta_stats, nombre_archivo), '-dpng', '-r200');
        close(fig_stat);
    end
end
