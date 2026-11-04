import unittest
from batch_sync import canonical,grade
class Rules(unittest.TestCase):
    def test_mirror_author_alias(self):
        self.assertEqual(canonical('https://flickr.com/photos/name/55541233666/'),canonical('https://www.flickr.com/photos/43669425%40N04/55541233666'))
    def test_missing_source_rejected(self):
        self.assertEqual(grade({'source_url':None}),'D')
    def test_wrong_products_rejected(self):
        for stock in ('Phoenix 125','HARMAN RED'):
            self.assertEqual(grade({'source_url':'x','film_stock':stock}),'D')
    def test_tags_not_verified_identity(self):
        self.assertEqual(grade({'source_url':'x','film_stock':'Phoenix II 200'}),'C')
    def test_unknown_scanner_does_not_exclude(self):
        self.assertEqual(grade({'source_url':'x','film_stock':'Phoenix 200','identity_verified':True,'scanner':'unknown'}),'B')
if __name__=='__main__':unittest.main()
