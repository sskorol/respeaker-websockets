module.exports = {
  apps : [{
    name: 'dsp',
    script: './respeaker_core',
    cwd: '/home/respeaker/projects/respeaker-websockets/build',
    instances: 1,
    error_file: './trace.log',
    out_file: './trace.log',
    merge_logs: true,
    cron_restart: '0 * * * *'
  }]
};
