import os
import unittest
from .helpers.ptrack_helpers import ProbackupTest
import subprocess
from time import sleep

module_name = 'time_consuming'

class TimeConsumingTests(ProbackupTest, unittest.TestCase):
    def test_pbckp150(self):
        """
        https://jira.postgrespro.ru/browse/PBCKP-150
        create a node filled with pgbench
        create FULL backup followed by PTRACK backup
        run pgbench, vacuum VERBOSE FULL and ptrack backups in parallel
        """
        # init node
        fname = self.id().split('.')[3]
        node = self.make_simple_node(
            base_dir=os.path.join(module_name, fname, 'node'),
            set_replication=True,
            initdb_params=['--data-checksums'])
        node.append_conf('postgresql.conf',
        """
            max_connections = 100
            wal_keep_size = 16000
            ptrack.map_size = 1
            shared_preload_libraries='ptrack'
            log_statement = 'none'
            fsync = off
            log_checkpoints = on
            autovacuum = off
        """)

        # init probackup and add an instance
        backup_dir = os.path.join(self.tmp_path, module_name, fname, 'backup')
        self.init_pb(backup_dir)
        self.add_instance(backup_dir, 'node', node)

        # run the node and init ptrack
        node.slow_start()
        node.safe_psql("postgres", "CREATE EXTENSION ptrack")
        # populate it with pgbench
        node.pgbench_init(scale=5)

        # FULL backup followed by PTRACK backup
        self.backup_node(backup_dir, 'node', node, options=['--stream'])
        self.backup_node(backup_dir, 'node', node, backup_type='ptrack', options=['--stream'])

        # run ordinary pgbench scenario to imitate some activity and another pgbench for vacuuming in parallel
        nBenchDuration = 30
        pgbench = node.pgbench(options=['-c', '20', '-j', '8', '-T', str(nBenchDuration)])
        with open('/tmp/pbckp150vacuum.sql', 'w') as f:
            f.write('VACUUM (FULL) pgbench_accounts, pgbench_tellers, pgbench_history; SELECT pg_sleep(1);\n')
        pgbenchval = node.pgbench(options=['-c', '1', '-f', '/tmp/pbckp150vacuum.sql', '-T', str(nBenchDuration)])

        # several PTRACK backups
        for i in range(nBenchDuration):
            print("[{}] backing up PTRACK diff...".format(i+1))
            self.backup_node(backup_dir, 'node', node, backup_type='ptrack', options=['--stream', '--log-level-console', 'VERBOSE'])
            sleep(0.1)
            # if the activity pgbench has finished, stop backing up
            if pgbench.poll() is not None:
                break

        pgbench.kill()
        pgbenchval.kill()
        pgbench.wait()
        pgbenchval.wait()

        backups = self.show_pb(backup_dir, 'node')
        for b in backups:
            self.assertEqual("OK", b['status'])

        # Clean after yourself
        self.del_test_dir(module_name, fname)
