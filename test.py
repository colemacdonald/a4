#!/usr/bin/env python3

# git stash && git pull && make && chmod 700 test.py && ./test.py

import os
import sys
import shutil
import unittest
import subprocess

################################################################################
# CONFIGURATION
################################################################################

# temporary dir to use for testing
testDir  = "./python_tmp_unittest/"
# dir containing disk images, outputs, originals
imageDir = "./IMAGES/"
# binary name of  each program
statuvfs = "./statuvfs"
lsuvfs   = "./lsuvfs"
catuvfs  = "./catuvfs"
storuvfs = "./storuvfs"
# set to false if the diff output is not enough to figure out why a test
# is failing
cleanup = True

################################################################################

def yes(prompt):
    return input(prompt + ' (y/n)').lower() in ['y', 'yes']

class TestUvfs(unittest.TestCase):

    def setUp(self):
        if os.path.isdir(testDir):
            if cleanup and not yes(testDir + ' already exists, continue?'):
                exit()
        else:
            os.makedirs(testDir)

    def tearDown(self):
        if cleanup:
            shutil.rmtree(testDir)

    def actual(self, args):
        return (testDir + '/actual[' + '_'.join(args)
            .replace('.', '')
            .replace('/', '') + ']')

    def expected(self, args):
        return (testDir + '/expected[' + '_'.join(args)
            .replace('.', '')
            .replace('/', '') + ']')

    def run_test(self, args, expected, diffoptions=[]):
        if not os.path.isfile(expected):
            self.assertTrue(os.path.isdir(testDir))
            with open(self.expected(args), 'w') as file:
                file.write(expected)
            expected = self.expected(args)
        with open(self.actual(args), 'w') as file:
            subprocess.call(args, stdout=file)
        self.assertEqual(0, subprocess.call(
            ['diff'] + diffoptions + [expected, self.actual(args)],
            stdout=sys.stdout
        ))

    def statuvfs_test(self, image, index):
        with open(imageDir + '/STAT_output.txt') as file:
            source = file.read()
        self.run_test(
            [statuvfs, '--image', imageDir + '/' + image],
            source
                .split('=' * 65)[index]
                .replace(image, imageDir + '/' + image)
                .strip(),
            ['-w']
        )

    def test_statuvfs_disk01X(self):
        self.statuvfs_test('disk01X.img', 0)
    def test_statuvfs_disk02X(self):
        self.statuvfs_test('disk02X.img', 1)
    def test_statuvfs_disk03X(self):
        self.statuvfs_test('disk03X.img', 2)
    def test_statuvfs_disk03(self):
        self.statuvfs_test('disk03.img',  3)
    def test_statuvfs_disk04X(self):
        self.statuvfs_test('disk04X.img', 4)
    def test_statuvfs_disk04(self):
        self.statuvfs_test('disk04.img',  5)
    def test_statuvfs_disk05X(self):
        self.statuvfs_test('disk05X.img', 6)
    def test_statuvfs_disk05(self):
        self.statuvfs_test('disk05.img',  7)

    def test_statuvfs_exit_1_no_args(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [statuvfs], stdout=fnull, stderr=fnull
            ))
    def test_statuvfs_exit_1_no_file(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [statuvfs, '--image', imageDir],
                stdout=fnull, stderr=fnull
            ))
    def test_statuvfs_exit_1_bad_image(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [statuvfs, '--image', './test.py'],
                stdout=fnull, stderr=fnull
            ))

    def lsuvfs_test(self, image, index=None):
        with open(imageDir + '/LS_output.txt') as file:
            source = (file
                .read()
                .replace('lsuvfs output for disk3.img', '')
                .replace('lsuvfs output for disk4.img', '')
                .replace('lsuvfs output for disk5.img', '')
            )
        self.run_test(
            [lsuvfs, '--image', imageDir + '/' + image],
            '' if index is None else source
                .split('=' * 67)[index]
                .replace(image, imageDir + '/' + image)
                .strip(),
            ['-w']
        )
    def test_lsuvfs_disk01X(self):
        self.lsuvfs_test('disk01X.img')
    def test_lsuvfs_disk02X(self):
        self.lsuvfs_test('disk02X.img')
    def test_lsuvfs_disk03X(self):
        self.lsuvfs_test('disk03X.img')
    def test_lsuvfs_disk03(self):
        self.lsuvfs_test('disk03.img', 0)
    def test_lsuvfs_disk04X(self):
        self.lsuvfs_test('disk04X.img')
    def test_lsuvfs_disk04(self):
        self.lsuvfs_test('disk04.img', 1)
    def test_lsuvfs_disk05X(self):
        self.lsuvfs_test('disk05X.img')
    def test_lsuvfs_disk05(self):
        self.lsuvfs_test('disk05.img', 2)

    def test_lsuvfs_exit_1_no_args(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [lsuvfs], stdout=fnull, stderr=fnull
            ))
    def test_lsuvfs_exit_1_no_file(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [lsuvfs, '--image', imageDir],
                stdout=fnull, stderr=fnull
            ))
    def test_lsuvfs_exit_1_bad_image(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [lsuvfs, '--image', './test.py'],
                stdout=fnull, stderr=fnull
            ))

    def catuvfs_test(self, image, filename):
        self.run_test(
            [catuvfs, '--image', imageDir + '/' + image, '--file', filename],
            imageDir + '/originals/' + filename
        )

    def test_catuvfs_disk03_alphabet_short(self):
        self.catuvfs_test('disk03.img', 'alphabet_short.txt')
    def test_catuvfs_disk03_digits_short(self):
        self.catuvfs_test('disk03.img', 'digits_short.txt')

    def test_catuvfs_disk04_alphabet_short(self):
        self.catuvfs_test('disk04.img', 'alphabet_short.txt')
    def test_catuvfs_disk04_digits_short(self):
        self.catuvfs_test('disk04.img', 'digits_short.txt')
    def test_catuvfs_disk04_alphabet(self):
        self.catuvfs_test('disk04.img', 'alphabet.txt')
    def test_catuvfs_disk04_digits(self):
        self.catuvfs_test('disk04.img', 'digits.txt')

    def test_catuvfs_disk05_sonnet116(self):
        self.catuvfs_test('disk05.img', 'sonnet116.txt')
    def test_catuvfs_disk05_graphic01(self):
        self.catuvfs_test('disk05.img', 'graphic01.jpg')
    def test_catuvfs_disk05_donne(self):
        self.catuvfs_test('disk05.img', 'donne.txt')
    def test_catuvfs_disk05_graphic02(self):
        self.catuvfs_test('disk05.img', 'graphic02.jpg')
    def test_catuvfs_disk05_sonnet023(self):
        self.catuvfs_test('disk05.img', 'sonnet023.txt')
    def test_catuvfs_disk05_macbeth(self):
        self.catuvfs_test('disk05.img', 'macbeth.txt')
    def test_catuvfs_disk05_graphic04(self):
        self.catuvfs_test('disk05.img', 'graphic04.jpg')
    def test_catuvfs_disk05_loves_labours_lost(self):
        self.catuvfs_test('disk05.img', 'loves_labours_lost.txt')
    def test_catuvfs_disk05_graphic03(self):
        self.catuvfs_test('disk05.img', 'graphic03.jpg')
    def test_catuvfs_disk05_random01(self):
        self.catuvfs_test('disk05.img', 'random01.bin')
    def test_catuvfs_disk05_sonnet018(self):
        self.catuvfs_test('disk05.img', 'sonnet018.txt')

    def test_catuvfs_exit_1_no_args(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [catuvfs], stdout=fnull, stderr=fnull
            ))
    def test_catuvfs_exit_1_no_file(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [catuvfs, '--image', imageDir, '--file', imageDir],
                stdout=fnull, stderr=fnull
            ))
    def test_catuvfs_exit_1_bad_image(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [catuvfs, '--image', './test.py', '--file', imageDir],
                stdout=fnull, stderr=fnull
            ))
    def test_catuvfs_exit_1_not_found(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [catuvfs, '--image', imageDir + '/disk01X.img', '--file', 'digits.txt'],
                stdout=fnull, stderr=fnull
            ))

    def storuvfs_test(self, image, filename, source=None):
        if source is None:
            source = imageDir + '/originals/' + filename
        subprocess.call([storuvfs,
            '--image', imageDir + '/' + image,
            '--file', filename,
            '--source', source
        ])
        self.run_test([catuvfs,
            '--image', imageDir + '/' + image,
            '--file', filename
        ], source)

    def test_storuvfs_disk01X_test(self):
        self.storuvfs_test('disk01X.img', 'test.py', './test.py')

    def test_storuvfs_disk03X_digits_short(self):
        self.storuvfs_test('disk03X.img', 'digits_short.txt')
    def test_storuvfs_disk03X_digits_short(self):
        self.storuvfs_test('disk03X.img', 'alphabet_short.txt')

    def test_storuvfs_disk04X_digits_short(self):
        self.storuvfs_test('disk04X.img', 'digits_short.txt')
    def test_storuvfs_disk04X_alphabet_short(self):
        self.storuvfs_test('disk04X.img', 'alphabet_short.txt')
    def test_storuvfs_disk04X_digits(self):
        self.storuvfs_test('disk04X.img', 'digits.txt')
    def test_storuvfs_disk04X_alphabet(self):
        self.storuvfs_test('disk04X.img', 'alphabet.txt')

    def test_storuvfs_disk04X_random01(self):
        self.storuvfs_test('disk04X.img', 'random01.bin')

    def test_storuvfs_disk05X_sonnet116(self):
        self.storuvfs_test('disk05X.img', 'sonnet116.txt')
    def test_storuvfs_disk05X_graphic01(self):
        self.storuvfs_test('disk05X.img', 'graphic01.jpg')
    def test_storuvfs_disk05X_donne(self):
        self.storuvfs_test('disk05X.img', 'donne.txt')
    def test_storuvfs_disk05X_graphic02(self):
        self.storuvfs_test('disk05X.img', 'graphic02.jpg')
    def test_storuvfs_disk05X_sonnet023(self):
        self.storuvfs_test('disk05X.img', 'sonnet023.txt')
    def test_storuvfs_disk05X_macbeth(self):
        self.storuvfs_test('disk05X.img', 'macbeth.txt')
    def test_storuvfs_disk05X_graphic04(self):
        self.storuvfs_test('disk05X.img', 'graphic04.jpg')
    def test_storuvfs_disk05X_loves_labours_lost(self):
        self.storuvfs_test('disk05X.img', 'loves_labours_lost.txt')
    def test_storuvfs_disk05X_graphic03(self):
        self.storuvfs_test('disk05X.img', 'graphic03.jpg')
    def test_storuvfs_disk05X_random01(self):
        self.storuvfs_test('disk05X.img', 'random01.bin')
    def test_storuvfs_disk05X_sonnet018(self):
        self.storuvfs_test('disk05X.img', 'sonnet018.txt')

    def test_storuvfs_exit_1_no_args(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [storuvfs], stdout=fnull, stderr=fnull
            ))
    def test_storuvfs_exit_1_no_file(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [storuvfs, '--image', imageDir, '--file', imageDir],
                stdout=fnull, stderr=fnull
            ))
    def test_storuvfs_exit_1_bad_image(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [storuvfs, '--image', './test.py', '--file', imageDir],
                stdout=fnull, stderr=fnull
            ))
    def test_sortuvfs_exit_1_long_filename(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [storuvfs, '--image', imageDir + '/disk01X.img', '--file', 'a' * 32, '--source', './test.py'],
                stdout=fnull, stderr=fnull
            ))
    def test_sortuvfs_exit_1_no_source(self):
        with open(os.devnull, 'w') as fnull:
            self.assertEqual(1, subprocess.call(
                [storuvfs, '--image', imageDir + '/disk01X.img', '--file', 'test', '--source', 'not-a-file'],
                stdout=fnull, stderr=fnull
            ))

if __name__ == '__main__':
    unittest.main()