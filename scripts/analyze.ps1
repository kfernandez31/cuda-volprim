$extensions = @("*.cpp", "*.c", "*.cc", "*.cxx", "*.h", "*.hpp", "*.hh", "*.cu")
$directories = @("src", "include")

Write-Host "Running cppcheck on: $($directories -join ', ')"
Write-Host ""

cppcheck `
    --enable=all `
    --inconclusive `
    --std=c++20 `
    --language=c++ `
    --inline-suppr `
    --force `
    --quiet `
    --suppress=missingIncludeSystem `
    -I include -I src `
    --project=build/compile_commands.json `
    -i third_party

Write-Host ""
Write-Host "Running clang-tidy on: $($directories -join ', ')"
Write-Host ""

# Gather all matching source files
$sourceFiles = foreach ($dir in $directories) {
    foreach ($ext in $extensions) {
        Get-ChildItem -Path $dir -Recurse -Include $ext -File
    }
}

Write-Host "-> Found $($sourceFiles.Count) files to analyze"
Write-Host ""

foreach ($file in $sourceFiles) {
    Write-Host "-> Analyzing $($file.FullName)"
    
    # Run clang-tidy and suppress "no compilation database" messages
    $output = & clang-tidy $file.FullName -p build 2>&1 | Where-Object {
        $_ -notmatch "warning: no compilation database found"
    }

    if ($output) {
        $output | Write-Output
        Write-Host "clang-tidy reported issues in: $($file.FullName)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "Static analysis complete."