function graficar_envolvente(envolvente, nombre_base, dir_notas)
% GRAFICAR_ENVOLVENTE Guarda la gráfica 2D de la envolvente normalizada.

    carpeta_env = fullfile(dir_notas, 'graficos_envolvente');
    if ~exist(carpeta_env, 'dir'), mkdir(carpeta_env); end

    fig_env = figure('visible', 'off');
    plot(envolvente, 'r-o'); grid on;
    title(['Envolvente Normalizada - ', nombre_base]);
    ylim([0 1.05]);
    xlabel('Índice Remuestreado'); ylabel('Amplitud Normalizada');

    ruta_env = fullfile(carpeta_env, [nombre_base, '_Envolvente.png']);
    print(fig_env, ruta_env, '-dpng', '-r200');
    close(fig_env);
end
