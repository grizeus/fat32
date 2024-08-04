const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_arch = .x86_64,
            .os_tag = .linux,
            .abi = .gnu,
        },
    });
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "fat32",
        .target = target,
        .optimize = optimize,
    });

    const c_files = [_][]const u8{
        "bootsec.c",
        "cd.c",
        "create_disk.c",
        "directory.c",
        "format_disk.c",
        "ls.c",
        "main.c",
        "mkdir.c",
        "utility.c",
        "touch.c",
    };

    for (c_files) |file| {
        exe.addCSourceFile(.{
            .file = .{ .cwd_relative = file },
            .flags = &[_][]const u8{ "-Wall", "-Wextra" },
        });
    }

    exe.linkLibC();

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const clean_step = b.step("clean", "Clean build artifacts");
    const clean_cmd = b.addSystemCommand(&.{ "rm", "-rf", "zig-cache", "zig-out" });
    clean_step.dependOn(&clean_cmd.step);
}
