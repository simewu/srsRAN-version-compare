data = readmatrix('logDirectoryOutput.csv');
data2 = readmatrix('Algorithm_benchmark_CI_output.csv')
data_str = readtable('logDirectoryOutput.csv');

% plotNum = 0 = File distribution
% plotNum = 1 = Percent code files / bytes changed
% plotNum = 2 = Proof Generation
% plotNum = 3 = Proof Verification
plotNum = 0


fontSize = 28;
f = figure;


xoffset = 0.3

if plotNum == 0
    x_str = table2array(data_str(1:end, 1))
    x = 1:length(x_str)
    y = data2(1:end, 10);
    extensions = table2array(data_str(1:end, 21))
    
    hold on;
    %otherBar = bar(x + xoffset, y, 'FaceColor', '#FFF')

    
    for i=1:length(x)
        plotExtensionHistogram(i + xoffset, y(i), extensions(i), {"c" "cc" "cpp" "h", "sh"}, {"#292929" "#5C5C5C" "#A1A1A1" "#D6D6D6", "#EEEEEE", "#FFF"})
    end

    %xticks(1:length(x))
    %xticklabels(x)

    xticks(x)
    xticklabels(x_str)
    
    xlabel('srsRAN Version')
    ylabel('Number of Files')
    xtickangle(60);
    
    %axis square;
    xlim([1 - 0.6 + xoffset, length(y) + 0.6 + xoffset])
    ylim([0, 1900])
    set(gca, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XScale', 'log');
    %set(gca, 'XScale', 'log', 'XTick',x_avg, 'XTickLabel', x_avg, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XTick',x_avg, 'XTickLabel', x_avg_real, 'YGrid', 'on', 'YMinorGrid', 'on');
    set(gca,'FontSize', fontSize);
    
    %legend('Bytes of code', 'Number of files', 'Location', 'NorthWest', 'NumColumns', 2)
    % ax = gca
    % ax.XAxis.FontSize = fontSize;
    % ax.YAxis.FontSize = fontSize;

elseif plotNum == 1
    % "rows = 2:end" is because this is a difference plot,
    % so the first version is "N/A"
    x = strcat(table2array(data_str(1:end - 1, 1)) , " – " , table2array(data_str(2:end, 1)))
    y = data(2:end, 17);
    y_B = data(2:end, 19);
    
    hold on;
    %bar([y, y_B], 'FaceColor', '#444');
    xaltoffset = 0.2
    for i=1:length(y)
        %if y_B(i) < y(i)
        bar(i - xaltoffset + xoffset, y(i), 'BarWidth', 0.4, 'FaceColor', '#444');
        bar(i + xaltoffset + xoffset, y_B(i), 'BarWidth', 0.4, 'FaceColor', '#BBB');
        %else
        %    bar(i, y_B(i), 'BarWidth', 0.4, 'FaceColor', '#888');
        %    bar(i, y(i), 'BarWidth', 0.4, 'FaceColor', '#444');
        %end
    end
    xticks(1:length(x))
    xticklabels(x)
    
    xlabel('srsRAN Version')
    ylabel('Difference (%)')
    xtickangle(60);
    
    %axis square;
    xlim([1 - 0.6 + xoffset, length(y) + 0.6 + xoffset])
    ylim([0, 1.15])
    set(gca, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XScale', 'log', 'XTick',x_avg, 'XTickLabel', x_avg, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XTick',x_avg, 'XTickLabel', x_avg_real, 'YGrid', 'on', 'YMinorGrid', 'on');
    set(gca,'FontSize', fontSize);
    
    legend('Number of files', 'Bytes of code', 'Location', 'NorthWest', 'NumColumns', 2)
    % ax = gca
    % ax.XAxis.FontSize = fontSize;
    % ax.YAxis.FontSize = fontSize;

elseif plotNum == 2
    
    x_str = table2array(data_str(1:end, 1))
    x = 1:length(x_str)

    read_files_y = data2(:, 2);
    read_files_y_ci = data2(:, 3);
    make_tree_y = data2(:, 4);
    make_tree_y_ci = data2(:, 5);
    generate_proof_y = data2(:, 6);
    generate_proof_y_ci = data2(:, 7);
    verify_proof_y = data2(:, 8);
    verify_proof_y_ci = data2(:, 9);

    tree_construction = read_files_y + make_tree_y
    tree_construction_ci = read_files_y_ci + make_tree_y_ci
    
    hold on;
    bar(x + xoffset, tree_construction, 'FaceColor', '#0593A2');
%     bar(read_files_y + make_tree_y + generate_proof_y + verify_proof_y, 'FaceColor', '#F00');
%     bar(read_files_y + make_tree_y + generate_proof_y, 'FaceColor', '#0F0');
%     bar(read_files_y + make_tree_y, 'FaceColor', '#00F');
%     bar(read_files_y, 'FaceColor', '#FFF');

%     plot(x, tree_construction, 'Color', 'Blue')
%     plot(x, generate_proof_y, 'Color', 'Blue')
%     plot(x, verify_proof_y, 'Color', 'Blue')
%     %plot(make_tree_y, 'Color', 'Red')
%     %plot(generate_proof_y, 'Color', 'Green')
%     %plot(verify_proof_y, 'Color', 'Yellow')
%     
    for i = 1:length(x)
        hold on;
        plotConfidenceInterval(i + xoffset, tree_construction(i), tree_construction_ci(i))
    end

    xlim([1 - 0.6 + xoffset, length(x) + 0.6 + xoffset])
    ylim([40, 240])
    yticks([40:20:240])
    
    %set(gca, 'YScale', 'log');
    %bar(y, 'FaceColor', '#444');
    xticks(x)
    xticklabels(x_str)
    
    xlabel('srsRAN Version')
    ylabel('Time (ms)')
    xtickangle(60);
    
    %axis square;
    %ylim([0, 1])
    set(gca, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XScale', 'log', 'XTick',x_avg, 'XTickLabel', x_avg, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XTick',x_avg, 'XTickLabel', x_avg_real, 'YGrid', 'on', 'YMinorGrid', 'on');
    set(gca,'FontSize', fontSize);
    
    %legend('Tree Construction', 'Proof generation', 'Proof verification', 'Location', 'NorthWest', 'NumColumns', 1)
    % ax = gca
    % ax.XAxis.FontSize = fontSize;
    % ax.YAxis.FontSize = fontSize;
    

elseif plotNum == 3
    
    x_str = table2array(data_str(1:end, 1))
    x = 1:length(x_str)

    read_files_y = data2(:, 2);
    read_files_y_ci = data2(:, 3);
    make_tree_y = data2(:, 4);
    make_tree_y_ci = data2(:, 5);
%     generate_proof_y = data2(:, 6);
%     generate_proof_y_ci = data2(:, 7);
    verify_proof_y = data2(:, 8);
    verify_proof_y_ci = data2(:, 9);

    tree_construction = read_files_y + make_tree_y
    tree_construction_ci = read_files_y_ci + make_tree_y_ci
    
    hold on;
    %bar(generate_proof_y + verify_proof_y, 'FaceColor', '#F2668B');
    bar(x + xoffset, verify_proof_y, 'FaceColor', '#FF7A48');
%     for i = 1:length(x)
%         hold on;
%         plotConfidenceInterval(i, generate_proof_y(i) + verify_proof_y(i), generate_proof_y_ci(i))
%     end
    for i = 1:length(x)
        hold on;
        plotConfidenceInterval(i + xoffset, verify_proof_y(i), verify_proof_y_ci(i))
    end
    xlim([1 - 0.6 + xoffset, length(x) + 0.6 + xoffset])
    %ylim([70, 190])
    ylim([0.03, 0.12])
    yticks([0.03:0.01:0.12])

    xticks(x)
    xticklabels(x_str)
    
    xlabel('srsRAN Version')
    ylabel('Time (ms)')
    xtickangle(60);
    
    %axis square;
    %ylim([0, 1])
    set(gca, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XScale', 'log', 'XTick',x_avg, 'XTickLabel', x_avg, 'YGrid', 'on', 'YMinorGrid', 'on');
    %set(gca, 'XTick',x_avg, 'XTickLabel', x_avg_real, 'YGrid', 'on', 'YMinorGrid', 'on');
    set(gca,'FontSize', fontSize);
    
    %legend('Proof Verification', 'Location', 'NorthWest', 'NumColumns', 1)
    % ax = gca
    % ax.XAxis.FontSize = fontSize;
    % ax.YAxis.FontSize = fontSize;
    

end

if plotNum == 1
    f.Position = [100 100 1500 800 + 170];
else
    f.Position = [100 100 1500 800];
end

function plotConfidenceInterval(i, y, yci)
    ci_color = 'black'
    ci_width = 0.2
    line_thickness = 1
    hold on;
    plot([i - ci_width, i + ci_width], [y - yci, y - yci], 'Color', ci_color, 'LineWidth', line_thickness, 'HandleVisibility','off');
    plot([i - ci_width, i + ci_width], [y + yci, y + yci], 'Color', ci_color, 'LineWidth', line_thickness, 'HandleVisibility','off');
    plot([i, i], [y - yci, y + yci], 'Color', ci_color, 'LineWidth', line_thickness, 'HandleVisibility','off');
    
    % Add a dot in the center
    % plot(x, y, '.', 'Color', ci_color, 'MarkerSize', line_thickness*10, 'HandleVisibility', 'off')
end

function plotExtensionHistogram(x, y, extensionString, ordering, colors)
    extensionStructStr = strcat('{', regexprep(extensionString, '\.?([^ ]*) \(([0-9]+)\)', '"$1":$2'), '}')
    disp(extensionStructStr{1})
    extensionStruct = jsondecode(extensionStructStr{1})

    bars = []
    totalFiles = 0
    keys = fieldnames(extensionStruct)
    for i=1:length(keys)
        num = extensionStruct.(keys{i})
        totalFiles = totalFiles + num
    end
    otherBar = bar(x, totalFiles, 'FaceColor', '#FFF')

    sum = 0
    for i=1:length(ordering)
        e = ordering{i}
        extensionValue = 0
        if any(strcmp(fieldnames(extensionStruct), e))
            extensionValue = extensionStruct.(e)
        end
        sum = sum + extensionValue
    end
    for i=1:length(ordering)
        e = ordering{i}
        extensionValue = 0
        if any(strcmp(fieldnames(extensionStruct), e))
            extensionValue = extensionStruct.(e)
        end
        b = bar(x, sum, 'FaceColor', colors{i})
        bars = [bars b]
        sum = sum - extensionValue
        ordering{i} = strcat('.', ordering{i})
    end

    text(x, y + 10, int2str(y), 'HorizontalAlignment', 'center', 'VerticalAlignment', 'baseline', 'FontSize', 14)
    text(x, totalFiles + 10, int2str(totalFiles), 'HorizontalAlignment', 'center', 'VerticalAlignment', 'baseline', 'FontSize', 14)

    bars = [bars otherBar]
    ordering = [ordering "Other files"]
    legend(bars, ordering, 'Location', 'NorthWest', 'NumColumns', 2)
end