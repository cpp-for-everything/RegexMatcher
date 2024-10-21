module.exports = ({ github, context }) => {
    const fs = require('fs');
    const path = require('path');

    // Recursively get all coverage-*/coverage.json files from the directory
    function getAllCoverageFiles(dir) {
        let files = [];

        fs.readdirSync(dir).forEach(file => {
            const fullPath = path.join(dir, file);
            console.log(dir, file, '=', fullPath);
            if (file.startsWith('coverage-')) {
                files.push(fullPath);
                console.log('Approved:', fullPath);
            }
        });

        return files;
    }

    // Get all coverage files from the directory
    const coverageFiles = getAllCoverageFiles('coverage-files/');
    console.log('Coverage files:', coverageFiles);

    // Extract unique uncovered lines and annotations
    function getUniqueCoverageData(coverageFiles) {
        const aggregated_data = {
            "files": new Set(),
            "functions": new Set(),
            "totals": new Set()
        };

        coverageFiles.forEach(file => {
            const data = JSON.parse(fs.readFileSync(file + "/coverage.json", 'utf8'));
            data['data'][0]['files'].forEach(file => aggregated_data["files"].add(file));
            data['data'][0]['functions'].forEach(function_data => aggregated_data["functions"].add(function_data));
            aggregated_data["totals"].add(data['data'][0]['totals']);
        });

        return aggregated_data;
    }

    const aggregated_data = getUniqueCoverageData(coverageFiles);

    // Extract unique annotations
    function getUniqueUncoveredLines(aggregated_data) {
        const uncoveredLinesSet = new Set();
        const annotations = [];

        aggregated_data.files.forEach(file => {
            if (file.segments && Array.isArray(file.segments)) {
                const fileName = file.filename;
                file.segments.forEach(segment => {
                    const [line, col, count] = segment;
                    const key = `${fileName}:${line}`; // Unique key based on filename and line number
                    if (count === 0 && !uncoveredLinesSet.has(key)) {
                        uncoveredLinesSet.add(key);
                        annotations.push({
                            path: fileName,
                            start_line: line,
                            end_line: line,
                            annotation_level: 'warning',
                            message: 'Uncovered line'
                        });
                    }
                });
            }
        });

        return annotations;
    }

    // Get unique uncovered lines from the combined coverage
    const annotations = getUniqueUncoveredLines(aggregated_data);

    // Send annotations to the pull request (once per uncovered line)
    annotations.forEach(annotation => {
        github.rest.checks.create({
            owner: context.repo.owner,
            repo: context.repo.repo,
            name: 'Code Coverage Check',
            head_sha: context.sha,
            output: {
                title: 'Code Coverage Report',
                summary: 'Found uncovered lines',
                annotations: [annotation],
            },
        });
    });

    // Function to calculate overall and file-specific coverage
    function calculateCoverage(aggregated_data) {
        let fileCoverage = [];
        let totalLines = 0;
        let coveredLines = 0;
        let processedFiles = new Set();  // To avoid duplicate files

        aggregated_data.files.forEach(file => {
            if (!processedFiles.has(file.filename)) {
                const fileSummary = file.summary.lines;

                // Extract per-file coverage percentage
                fileCoverage.push({
                    filename: file.filename,
                    coveragePercentage: fileSummary.percent.toFixed(2),
                    totalLines: fileSummary.count,
                    coveredLines: fileSummary.covered
                });

                totalLines += fileSummary.count;
                coveredLines += fileSummary.covered;
                processedFiles.add(file.filename); // Mark file as processed
            }
        });

        const overallCoverage = (coveredLines / totalLines) * 100;

        return { overallCoverage: overallCoverage.toFixed(2), fileCoverage };
    }

    // Calculate overall and per-file coverage
    const { overallCoverage, fileCoverage } = calculateCoverage(aggregated_data);

    // Build the coverage summary message
    let summary = `### Code Coverage Summary\n`;
    summary += `**Overall Coverage**: ${overallCoverage}%\n\n`;
    summary += `#### File-by-file Coverage:\n`;

    fileCoverage.forEach(file => {
        summary += `- **${file.filename}**: ${file.coveragePercentage}% (${file.coveredLines}/${file.totalLines} lines covered)\n`;
    });

    // Post the summary as a comment on the pull request
    github.rest.issues.createComment({
        owner: context.repo.owner,
        repo: context.repo.repo,
        issue_number: context.payload.pull_request.number,
        body: summary,
    });
}