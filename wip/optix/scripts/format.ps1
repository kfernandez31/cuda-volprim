$extensions = @("*.cpp", "*.c", "*.cc", "*.cxx", "*.h", "*.hpp", "*.hh", "*.cu")
$directories = @("src", "include", "device")

Write-Host "Running clang-format on: $directories"
Write-Host ""

foreach ($dir in $directories) {
    foreach ($ext in $extensions) {
        Get-ChildItem -Path $dir -Recurse -Include $ext -File | ForEach-Object {
            Write-Host "Formatting $($_.FullName)"
            clang-format -i $_.FullName
        }
    }
}

Write-Host ""
Write-Host "Done formatting!"